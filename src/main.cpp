// SPDX-License-Identifier: GPL-2.0-only

#include <cxxopts.hpp>
#include <iostream>

#include <sl/kerncvs/Branches.h>
#include <sl/kerncvs/CollectConfigs.h>
#include <sl/kerncvs/PatchesAuthors.h>
#include <sl/kerncvs/SupportedConf.h>
#include <sl/git/Git.h>
#include <sl/helpers/Color.h>
#include <sl/helpers/Misc.h>
#include <sl/helpers/Process.h>
#include <sl/helpers/PushD.h>
#include <sl/helpers/String.h>

#include "sql/F2CSQLConn.h"
#include "treewalker/ConsoleMakeVisitor.h"
#include "treewalker/SQLiteMakeVisitor.h"
#include "treewalker/TreeWalker.h"
#include "Verbose.h"

using Clr = SlHelpers::Color;

namespace {

struct Opts {
	std::vector<std::string> appendBranches;
	std::vector<std::string> branches;
	std::filesystem::path dest;
	bool hasDest;
	bool force;
	bool quiet;
	unsigned verbose;

	bool authorsDumpRefs;
	bool authorsReportUnhandled;

	std::filesystem::path sqlite;
	bool hasSqlite;
	bool sqliteCreate;
	bool sqliteCreateOnly;
};

Opts getOpts(int argc, char **argv)
{
	cxxopts::Options options { argv[0], "Generate conf_file_map database (and more)" };
	Opts opts;
	options.add_options()
		("a,append-branch", "process also this branch", cxxopts::value(opts.appendBranches))
		("b,branch", "branch to process", cxxopts::value(opts.branches))
		("force-color", "force color output")
		("dest", "destination (scratch area)",
			cxxopts::value(opts.dest)->default_value("$SCRATCH_AREA/fill-db"))
		("f,force", "force branch creation (delete old data)",
			cxxopts::value(opts.force)->default_value("false"))
		("q,quiet", "quiet mode", cxxopts::value(F2C::quiet)->default_value("false"))
		("v,verbose", "verbose mode")
	;
	options.add_options("authors")
		("authors-dump-refs", "dump references to stdout",
			cxxopts::value(opts.authorsDumpRefs)->default_value("false"))
		("authors-report-unhandled", "report unhandled lines to stdout",
			cxxopts::value(opts.authorsReportUnhandled)->default_value("false"))
	;
	options.add_options("sqlite")
		("s,sqlite", "create db",
			cxxopts::value(opts.sqlite)->implicit_value("conf_file_map.sqlite"))
		("S,sqlite-create", "create the db if not exists",
			cxxopts::value(opts.sqliteCreate)->default_value("false"))
		("O,sqlite-create-only", "only create the db (do not fill it)",
			cxxopts::value(opts.sqliteCreateOnly)->default_value("false"))
	;

	try {
		auto cxxopts = options.parse(argc, argv);
		F2C::verbose = cxxopts.count("verbose");
		Clr::forceColor(cxxopts.contains("force-color"));
		opts.hasDest = cxxopts.contains("dest");
		opts.hasSqlite = cxxopts.contains("sqlite");
		return opts;
	} catch (const cxxopts::exceptions::parsing &e) {
		Clr(std::cerr, Clr::RED) << "arguments error: " << e.what();
		std::cerr << options.help();
		exit(EXIT_FAILURE);
	}
}

std::optional<std::filesystem::path> prepareScratchArea(const Opts &opts)
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
	std::error_code ec;
	std::filesystem::create_directories(scratchArea, ec);
	if (ec) {
		Clr(std::cerr, Clr::RED) << __func__ << ": cannot create " << scratchArea <<
						": error=" << ec;
		return {};
	}

	return std::filesystem::absolute(scratchArea);
}

std::optional<SlGit::Repo> prepareKsourceGit(const std::filesystem::path &scratchArea)
{
	static const std::string kerncvs { "jslaby@kerncvs.suse.de:/srv/git/kernel-source.git" };

	auto ourKsourceGit = scratchArea / "kernel-source";

	if (std::filesystem::exists(ourKsourceGit))
		return SlGit::Repo::open(ourKsourceGit);

	auto repo = SlGit::Repo::init(ourKsourceGit, false, kerncvs);
	if (!repo) {
		Clr(std::cerr, Clr::RED) << __func__ << ": cannot init: " <<
						git_error_last()->message;
		return std::nullopt;
	}

	auto origin = repo->remoteLookup("origin");
	auto ret = origin->fetch("scripts", 1, false);
	if (ret) {
		Clr(std::cerr, Clr::RED) << __func__ << ": cannot fetch: " <<
						git_error_last()->message;
		return std::nullopt;
	}

	ret = repo->checkout("refs/remotes/origin/scripts");
	if (ret) {
		Clr(std::cerr, Clr::RED) << __func__ << ": cannot checkout: " <<
						git_error_last()->message;
		return std::nullopt;
	}

	std::error_code ec;
	SlHelpers::PushD push(ourKsourceGit, ec);
	if (ec)
		return std::nullopt;

	auto stat = std::system("./scripts/install-git-hooks");
	if (stat) {
		Clr(std::cerr, Clr::RED) << __func__ << ": cannot install hooks: " <<
						WEXITSTATUS(stat);
		return std::nullopt;
	}

	return repo;
}

std::unique_ptr<SQL::F2CSQLConn> getSQL(const Opts &opts)
{
	auto sql = std::make_unique<SQL::F2CSQLConn>();
	unsigned openFlags = 0;
	if (opts.sqliteCreate)
		openFlags |= SlSqlite::CREATE;
	if (!sql->openDB(opts.sqlite, openFlags))
		return {};
	if (opts.sqliteCreate) {
		if (!sql->createDB())
			return {};
	}
	if (!opts.sqliteCreateOnly) {
		if (!sql->prepDB())
			return {};
	}

	return sql;
}

std::string getBranchNote(const std::string &branch, const unsigned &branchNo,
			  const unsigned &branchCnt)
{
	auto percent = 100.0 * branchNo / branchCnt;
	std::stringstream ss;
	ss << branch << " (" << branchNo << "/" << branchCnt << " -- " <<
	      std::fixed << std::setprecision(2) << percent << " %)";
	return ss.str();
}

std::optional<bool> skipBranch(const std::unique_ptr<SQL::F2CSQLConn> &sql,
			       const std::string &branch, bool force)
{
	if (!sql)
		return false;

	if (force) {
		if (!sql->deleteBranch(branch))
			return {};
		return false;
	}

	return sql->hasBranch(branch);
}

std::optional<SlGit::Commit> checkoutBranch(const std::string &branchNote,
					    const std::string &branch,
					    const SlGit::Repo &repo)
{
	Clr(Clr::GREEN) << "== " << branchNote << " -- Checking Out ==";
	if (repo.checkout("refs/remotes/origin/" + branch)) {
		Clr(std::cerr, Clr::RED) << "Cannot check out '" << branch << "': " <<
					    git_error_last()->message;
		return std::nullopt;
	}

	return repo.commitRevparseSingle("HEAD");
}

std::filesystem::path getExpandedDir(const std::filesystem::path &scratchArea,
				     const std::string &branch)
{
	auto branchDir(branch);
	std::replace(branchDir.begin(), branchDir.end(), '/', '_');
	return scratchArea / branchDir;
}

bool expandBranch(const std::string &branchNote, const std::filesystem::path &scratchArea,
		  const std::filesystem::path &expandedTree)
{
	auto kernelSource = scratchArea / "kernel-source";
	std::error_code ec;
	SlHelpers::PushD push(kernelSource, ec);
	if (ec) {
		Clr(std::cerr, Clr::RED) << __func__ << ": cannot chdir to " << kernelSource;
		return false;
	}

	Clr(Clr::GREEN) << "== " << branchNote << " -- Expanding ==";

	std::filesystem::path seqPatch{"./scripts/sequence-patch"};
	// temporary for old branches
	if (!std::filesystem::exists(seqPatch)) {
		Clr(Clr::YELLOW) << "Running old sequence-patch.sh as sequence-patch does not exist";
		seqPatch = "./scripts/sequence-patch.sh";
	}
	const std::vector<std::string> args {
		"--dir=" + scratchArea.string(),
		"--patch-dir=" + expandedTree.string(),
		"--rapid",
	};
	SlHelpers::Process P;
	auto ret = P.run(seqPatch, args);
	if (F2C::verbose > 1)
		std::cout << "cmd=" << seqPatch << " stat=" << P.lastErrorNo() << '/' <<
			     P.exitStatus() << '\n';
	if (ret || P.exitStatus()) {
		Clr(std::cerr, Clr::RED) << __func__ << ": cannot seq patch: " <<
					    P.lastError() << " (" << P.exitStatus() << ')';
		return false;
	}

	return true;
}

std::unique_ptr<TW::MakeVisitor> getMakeVisitor(const std::unique_ptr<SQL::F2CSQLConn> &sql,
						const SlKernCVS::SupportedConf &supp,
						const std::string &branch,
						const std::filesystem::path &root)
{
	if (sql)
		return std::make_unique<TW::SQLiteMakeVisitor>(*sql, supp, branch, root);
	else
		return std::make_unique<TW::ConsoleMakeVisitor>();
}

std::optional<SlKernCVS::SupportedConf> getSupported(const SlGit::Repo &repo,
						     const SlGit::Commit &commit)
{
	auto suppConf = commit.catFile(repo, "supported.conf");
	if (!suppConf)
		return {};

	return SlKernCVS::SupportedConf { *suppConf };
}

bool processAuthors(const Opts &opts, const std::unique_ptr<SQL::F2CSQLConn> &sql,
		    const std::string &branch, const SlGit::Repo &repo, const SlGit::Commit &commit)
{
	SlKernCVS::PatchesAuthors PA{repo, opts.authorsDumpRefs, opts.authorsReportUnhandled};

	return PA.processAuthors(commit, [&sql](const std::string &email) -> bool {
		return sql->insertUser(email);
	}, [&branch, &sql](const std::string &email, const std::filesystem::path &path,
			unsigned count, unsigned realCount) -> bool {
		return sql->insertUFMap(branch, email, path.parent_path().string(),
				     path.filename().string(), count, realCount);
	});
}

bool processConfigs(const std::unique_ptr<SQL::F2CSQLConn> &sql, const std::string &branch,
		    const SlGit::Repo &repo, const SlGit::Commit &commit)
{
	SlKernCVS::CollectConfigs CC{repo,
		[&sql](const std::string &arch, const std::string &flavor) -> int {
			return sql->insertArch(arch) && sql->insertFlavor(flavor);
		}, [&sql, &branch](const std::string &arch, const std::string &flavor,
		      const std::string &config,
		      const SlKernCVS::CollectConfigs::ConfigValue &value) -> int {
			return sql->insertConfig(config) && sql->insertCBMap(branch, arch, flavor,
									     config,
									     std::string(1, value));
	}};

	return CC.collectConfigs(commit);
}

bool processBranch(const Opts &opts, const std::string &branchNote,
		   const std::unique_ptr<SQL::F2CSQLConn> &sql,
		   const std::string &branch, const SlGit::Repo &repo, SlGit::Commit &commit,
		   const std::filesystem::path &root)
{
	if (sql) {
		sql->begin();
		auto SHA = commit.idStr();
		if (!sql->insertBranch(branch, SHA)) {
			Clr(std::cerr, Clr::RED) << "cannot add branch '" << branch <<
							"' with SHA '" << SHA << '\'';
			return false;
		}
	}

	if (!opts.sqliteCreateOnly) {
		Clr(Clr::GREEN) << "== " << branchNote << " -- Retrieving supported info ==";
		auto supp = getSupported(repo, commit);
		if (!supp)
			return false;

		Clr(Clr::GREEN) << "== " << branchNote << " -- Running file2config ==";
		auto visitor = getMakeVisitor(sql, *supp, branch, root);
		TW::TreeWalker tw(root, *visitor);
		tw.walk();

		if (sql) {
			Clr(Clr::GREEN) << "== " << branchNote << " -- Collecting configs ==";
			if (!processConfigs(sql, branch, repo, commit))
				return false;

			Clr(Clr::GREEN) << "== " << branchNote <<
					       " -- Detecting authors of patches ==";
			if (!processAuthors(opts, sql, branch, repo, commit))
				return false;
		}
	}

	if (sql) {
		Clr(Clr::GREEN) << "== " << branchNote << " -- Committing ==";
		sql->end();
	}

	return true;
}

} // namespace

int main(int argc, char **argv)
{
	const auto opts = getOpts(argc, argv);

	Clr(Clr::GREEN) << "== Preparing trees ==";

	auto scratchArea = prepareScratchArea(opts);
	if (!scratchArea)
		return EXIT_FAILURE;

	auto repo = prepareKsourceGit(*scratchArea);
	if (!repo)
		return EXIT_FAILURE;

	SlKernCVS::Branches::BranchesList branches { std::move(opts.branches) };
	if (branches.empty()) {
		auto branchesOpt = SlKernCVS::Branches::getBuildBranches();
		if (!branchesOpt)
			return EXIT_FAILURE;
		branches = *branchesOpt;
	}

	branches.insert(branches.end(), opts.appendBranches.begin(), opts.appendBranches.end());

	Clr(Clr::GREEN) << "== Fetching branches ==";

	auto remote = repo->remoteLookup("origin");
	if (!remote)
		return EXIT_FAILURE;
	if (remote->fetchBranches(branches, 1, false)) {
		auto lastErr = git_error_last();
		Clr(std::cerr, Clr::RED) << "fetch failed: " << lastErr->message <<
						" (" << lastErr->klass << ')';
		return EXIT_FAILURE;
	}

	auto sql = opts.hasSqlite ? getSQL(opts) : nullptr;
	if (opts.hasSqlite && !sql)
		return EXIT_FAILURE;

	auto branchNo = 0U;
	auto branchCnt = branches.size();

	for (const auto &branch: branches) {
		auto branchNote = getBranchNote(branch, ++branchNo, branchCnt);
		Clr(Clr::GREEN) << "== " << branchNote << " -- Starting ==";
		auto skipOpt = skipBranch(sql, branch, opts.force);
		if (!skipOpt)
			return EXIT_FAILURE;
		if (*skipOpt) {
			Clr(Clr::YELLOW) << "Already present, skipping, use -f to force re-creation";
			continue;
		}

		auto branchCommit = checkoutBranch(branchNote, branch, *repo);
		if (!branchCommit)
			return EXIT_FAILURE;

		auto expandedTree = getExpandedDir(*scratchArea, branch);

		if (!expandBranch(branchNote, *scratchArea, expandedTree))
			return EXIT_FAILURE;

		if (!processBranch(opts, branchNote, sql, branch, *repo, *branchCommit,
				   expandedTree))
			return EXIT_FAILURE;
	}

	return 0;
}
