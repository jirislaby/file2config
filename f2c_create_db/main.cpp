// SPDX-License-Identifier: GPL-2.0-only

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

#include <sl/kerncvs/Branches.h>
#include <sl/git/Git.h>
#include <sl/helpers/Color.h>
#include <sl/helpers/Exception.h>
#include <sl/helpers/Misc.h>
#include <sl/helpers/Process.h>
#include <sl/helpers/PushD.h>
#include <sl/kerncvs/SupportedConf.h>
#include <sl/sqlite/SQLConn.h>

#include "F2CSQLConn.h"

#include "BranchProcessor.h"
#include "Opts.h"
#include "Renames.h"
#include "StatusNotifier.h"

using Clr = SlHelpers::Color;
using Json = nlohmann::ordered_json;
using RunEx = SlHelpers::RuntimeException;
using SlHelpers::raise;

using namespace F2C;

namespace {

std::filesystem::path prepareScratchArea(const Opts &opts)
{
	std::filesystem::path scratchArea;
	if (opts.hasDest) {
		scratchArea = opts.dest;
	} else if (auto scratchAreaEnv = SlHelpers::Env::get<std::filesystem::path>("SCRATCH_AREA")) {
		scratchArea = std::move(*scratchAreaEnv) / "fill-db";
	} else {
		Clr(std::cerr, Clr::YELLOW) << "Neither --dest, nor SCRATCH_AREA defined (defaulting to \"fill-db\")";
		scratchArea = "fill-db";
	}
	try {
		std::filesystem::create_directories(scratchArea);
	} catch (std::filesystem::filesystem_error &e) {
		RunEx(__func__) << ": cannot create " << scratchArea << ": error=" << e.what() <<
				" (" << e.code() << ')' << raise;
	}

	return std::filesystem::absolute(scratchArea);
}

SlGit::Repo prepareKsourceGit(const std::filesystem::path &scratchArea)
{
	static const std::string kerncvs { "jslaby@kerncvs.suse.de:/srv/git/kernel-source.git" };

	auto ourKsourceGit = scratchArea / "kernel-source";

	if (std::filesystem::exists(ourKsourceGit)) {
		if (auto repo = SlGit::Repo::open(ourKsourceGit))
			return std::move(*repo);
		RunEx(__func__) << ": cannot open: " << SlGit::Repo::lastError() << raise;
	}

	auto repo = SlGit::Repo::init(ourKsourceGit, false, kerncvs);
	if (!repo)
		RunEx(__func__) << ": cannot init: " << SlGit::Repo::lastError() << raise;

	auto origin = repo->remoteLookup("origin");
	if (!origin->fetch("scripts", 1, false))
		RunEx(__func__) << ": cannot fetch: " << repo->lastError() << raise;

	if (!repo->checkout("refs/remotes/origin/scripts"))
		RunEx(__func__) << ": cannot checkout: " << repo->lastError() << raise;

	std::error_code ec;
	SlHelpers::PushD push(ourKsourceGit, ec);
	if (ec)
		RunEx(__func__) << ": cannot chdir to " << ourKsourceGit << raise;

	SlHelpers::Process P;
	if (!P.run("./scripts/install-git-hooks") || P.exitStatus())
		RunEx(__func__) << ": cannot install hooks: " << P.lastError() <<
				   " (" << P.exitStatus() << ')' << raise;

	return std::move(*repo);
}

F2CSQLConn getSQL(const Opts &opts)
{
	F2CSQLConn sql;
	auto openFlags = SlSqlite::OpenFlags::NONE;
	if (opts.sqliteCreate)
		openFlags |= SlSqlite::OpenFlags::CREATE;
	if (!sql.openDB(opts.sqlite, openFlags))
		RunEx("Cannot open/create the db at ") << opts.sqlite << ": " << sql.lastError() <<
							  raise;

	if (opts.sqliteCreate && !sql.createDB())
		RunEx("Cannot create tables: ") << sql.lastError() << raise;

	if (!opts.sqliteCreateOnly && !sql.prepDB())
		RunEx("Cannot prepare statements: ") << sql.lastError() << raise;

	return sql;
}

void fillSupported(F2CSQLConn &sql)
{
	for (auto e: SlKernCVS::SupportStateRange{})
		if (!sql.insertSupported(static_cast<int>(e), std::string(SlKernCVS::getName(e))))
			RunEx("Cannot insert supported: ") << sql.lastError() << raise;
}

auto obtainBranches(const Opts &opts, const SlGit::Repo &repo,
		    const std::optional<Json> &configuration)
{
	SlKernCVS::Branches::BranchesList branches { opts.branches };
	if (branches.empty()) {
		auto branchesOpt = SlKernCVS::Branches::getBuildBranches();
		if (!branchesOpt)
			RunEx("Cannot download branches.conf").raise();
		branches = std::move(*branchesOpt);
	}

	// from command line
	branches.insert(branches.end(), opts.appendBranches.begin(), opts.appendBranches.end());

	// from configuration
	if (configuration && configuration->contains("append_branches")) {
		const auto confBranches = (*configuration)["append_branches"].get_ref<const Json::array_t &>();

		branches.insert(branches.end(), confBranches.begin(), confBranches.end());
	}

	if (!opts.noFetch) {
		Clr(Clr::GREEN) << "== Fetching branches ==";

		auto remote = repo.remoteLookup("origin");
		if (!remote)
			RunEx("No origin").raise();
		if (!remote->fetchBranches(branches, 1, false))
			RunEx("Fetch failed: ") << repo.lastError() << raise;
	}

	return branches;
}

std::optional<Json> loadConfiguration(const Opts &opts)
{
	if (!opts.hasConfiguration)
		return std::nullopt;

	std::ifstream ifs{opts.configurationJSON};
	if (!ifs)
		RunEx("Cannot open JSON: ") << opts.configurationJSON << raise;

	Json json;
	try {
		json = json.parse(ifs);
	} catch (const Json::exception &e) {
		RunEx("Cannot parse JSON from ") << opts.configurationJSON << ": " << e.what() <<
						    raise;
	}

	return json;
}

BranchProcessor::UserSet loadValidUsers(const Opts &opts)
{
	if (opts.authorsValidUsers.empty())
		return {};

	BranchProcessor::UserSet validUsers;
	std::ifstream ifs{opts.authorsValidUsers};
	if (!ifs)
		RunEx("Cannot open valid users file ") << opts.authorsValidUsers << ": " <<
			strerror(errno) << raise;

	for (std::string line; std::getline(ifs, line); ) {
		line = SlHelpers::String::trim(line);
		if (line.empty() || line.starts_with('#'))
			continue;
		validUsers.emplace(std::move(line));
	}

	return validUsers;
}

bool skipBranch(F2CSQLConn &sql, const std::string &branch, bool force)
{
	if (force) {
		if (!sql.deleteBranch(branch))
			RunEx("Cannot delete branch '") << branch << "': " << sql.lastError() <<
							  raise;
		return false;
	}

	return sql.hasBranch(branch);
}

void handleEx(int argc, char **argv)
{
	const auto opts = Opts::getOpts(argc, argv);

	auto configuration = loadConfiguration(opts);
	auto validUsers = loadValidUsers(opts);

	const auto lpath = SlHelpers::Env::get<std::filesystem::path>("LINUX_GIT");
	if (!lpath)
		RunEx("LINUX_GIT not set").raise();

	auto lrepo = SlGit::Repo::open(*lpath);
	if (!lrepo)
		RunEx("Cannot open LINUX_GIT repo: ") << SlGit::Repo::lastError() <<
							 " (" << SlGit::Repo::lastClass() << ')' <<
							 raise;

	Clr(Clr::GREEN) << "== Preparing trees ==";

	auto scratchArea = prepareScratchArea(opts);
	auto repo = prepareKsourceGit(scratchArea);
	auto branches = obtainBranches(opts, repo, configuration);
	auto sql = getSQL(opts);
	fillSupported(sql);

	auto branchNo = 0U;
	auto branchCnt = branches.size();

	BranchesProps branchesProps;
	for (const auto &branch: branches) {
		StatusNotifier notifier(branch, ++branchNo, branchCnt);

		notifier.notify("Starting");
		if (skipBranch(sql, branch, opts.force)) {
			Clr(Clr::YELLOW) << "Already present, skipping, use -f to force re-creation";
			continue;
		}

		BranchProcessor bp{branch, notifier, scratchArea, branchesProps, repo, sql, opts,
			configuration, validUsers};

		bp.process();
	}

	if (!opts.noRenames) {
		Clr(Clr::GREEN) << "== Collecting renames ==";
		Renames::processRenames(sql, *lrepo, branchesProps);

		if (!sql.exec("VACUUM;"))
			RunEx("Cannot VACUUM the DB: ") << sql.lastError() << raise;
	}
}

} // namespace

int main(int argc, char **argv)
{
	try {
		handleEx(argc, argv);
	} catch (std::runtime_error &e) {
		Clr(std::cerr, Clr::RED) << e.what();
		return EXIT_FAILURE;
	}

	return 0;
}
