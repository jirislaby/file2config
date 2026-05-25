// SPDX-License-Identifier: GPL-2.0-only

#include <cxxopts.hpp>
#include <fnmatch.h>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

#include <sl/kerncvs/Branches.h>
#include <sl/kerncvs/CollectConfigs.h>
#include <sl/kerncvs/PatchesAuthors.h>
#include <sl/kerncvs/SupportedConf.h>
#include <sl/git/Git.h>
#include <sl/helpers/Color.h>
#include <sl/helpers/Exception.h>
#include <sl/helpers/Misc.h>
#include <sl/helpers/Process.h>
#include <sl/helpers/PushD.h>

#include "parser/kconfig/Parser.h"
#include "sql/F2CSQLConn.h"
#include "treewalker/SQLiteMakeVisitor.h"
#include "treewalker/TreeWalker.h"

#include "BranchProps.h"
#include "Renames.h"
#include "Verbose.h"

using Clr = SlHelpers::Color;
using Json = nlohmann::ordered_json;
using RunEx = SlHelpers::RuntimeException;
using SlHelpers::raise;

using namespace F2C;

namespace {

struct Opts {
	std::vector<std::string> appendBranches;
	std::vector<std::string> branches;
	std::filesystem::path dest;
	bool hasDest;
	bool force;
	bool noFetch;
	bool noRenames;
	bool quiet;
	unsigned verbose;

	bool authorsDumpRefs;
	bool authorsReportUnhandled;

	std::filesystem::path configurationJSON;
	bool hasConfiguration;

	std::filesystem::path sqlite;
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
		("no-fetch", "work offline, no updates of repos",
			cxxopts::value(opts.noFetch)->default_value("false"))
		("no-renames", "do not detect and store file renames",
			cxxopts::value(opts.noRenames)->default_value("false"))
		("q,quiet", "quiet mode", cxxopts::value(F2C::quiet)->default_value("false"))
		("v,verbose", "verbose mode")
	;
	options.add_options("authors")
		("authors-dump-refs", "dump references to stdout",
			cxxopts::value(opts.authorsDumpRefs)->default_value("false"))
		("authors-report-unhandled", "report unhandled lines to stdout",
			cxxopts::value(opts.authorsReportUnhandled)->default_value("false"))
	;
	options.add_options("files")
		("configuration", "path to JSON containing configuration to be used",
			cxxopts::value(opts.configurationJSON))
	;
	options.add_options("sqlite")
		("s,sqlite", "db name",
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
		Clr::forceColorValue(cxxopts.contains("force-color"));
		opts.hasDest = cxxopts.contains("dest");
		opts.hasConfiguration = cxxopts.contains("configuration");
		return opts;
	} catch (const cxxopts::exceptions::parsing &e) {
		Clr(std::cerr, Clr::RED) << "arguments error: " << e.what();
		std::cerr << options.help();
		exit(EXIT_FAILURE);
	}
}

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

SQL::F2CSQLConn getSQL(const Opts &opts)
{
	SQL::F2CSQLConn sql;
	unsigned openFlags = 0;
	if (opts.sqliteCreate)
		openFlags |= SlSqlite::CREATE;
	if (!sql.openDB(opts.sqlite, openFlags))
		RunEx("Cannot open/create the db at ") << opts.sqlite << ": " << sql.lastError() <<
							  raise;

	if (opts.sqliteCreate && !sql.createDB())
		RunEx("Cannot create tables: ") << sql.lastError() << raise;

	if (!opts.sqliteCreateOnly && !sql.prepDB())
		RunEx("Cannot prepare statements: ") << sql.lastError() << raise;

	return sql;
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

std::string getBranchNote(const std::string &branch, unsigned branchNo, unsigned branchCnt)
{
	auto percent = 100.0 * branchNo / branchCnt;
	std::ostringstream ss;
	ss << branch << " (" << branchNo << "/" << branchCnt << " -- " <<
	      std::fixed << std::setprecision(2) << percent << " %)";
	return ss.str();
}

bool skipBranch(SQL::F2CSQLConn &sql, const std::string &branch, bool force)
{
	if (force) {
		if (!sql.deleteBranch(branch))
			RunEx("Cannot delete branch '") << branch << "': " << sql.lastError() <<
							  raise;
		return false;
	}

	return sql.hasBranch(branch);
}

SlGit::Commit checkoutBranch(const std::string &branchNote, const std::string &branch,
			     const SlGit::Repo &repo)
{
	Clr(Clr::GREEN) << "== " << branchNote << " -- Checking Out ==";
	if (!repo.checkout("refs/remotes/origin/" + branch))
		RunEx("Cannot check out '") << branch << "': " << repo.lastError() << raise;

	auto commit = repo.commitRevparseSingle("HEAD");
	if (!commit)
		RunEx("Cannot find HEAD: ") << repo.lastError() << raise;

	return std::move(*commit);
}

std::filesystem::path getExpandedDir(const std::filesystem::path &scratchArea, std::string branch)
{
	std::replace(branch.begin(), branch.end(), '/', '_');
	return scratchArea / branch;
}

void expandBranch(const std::string &branchNote, const std::filesystem::path &scratchArea,
		  const std::filesystem::path &expandedTree)
{
	auto kernelSource = scratchArea / "kernel-source";
	std::error_code ec;
	SlHelpers::PushD push(kernelSource, ec);
	if (ec)
		RunEx(__func__) << ": cannot chdir to " << kernelSource << raise;

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
	if (!ret || P.exitStatus())
		RunEx(__func__) << ": cannot seq patch: " << P.lastError() <<
				   " (" << P.exitStatus() << ')' << raise;
}

SlKernCVS::SupportedConf getSupported(const SlGit::Commit &commit)
{
	auto suppConf = commit.catFile("supported.conf");
	if (!suppConf)
		RunEx("Cannot obtain supported.conf: ") << commit.repo().lastError() << raise;

	return SlKernCVS::SupportedConf { *suppConf };
}

void insertConfigSQL(Kconfig::Config::Configs &configs, const Kconfig::Parser &p,
		     SQL::F2CSQLConn &sql)
{
	p.walkConfigs([&configs, &sql](auto conf, auto type) {
		auto realConf = "CONFIG_" + conf;
		if (!sql.insertConfig(realConf, static_cast<unsigned>(type)))
			RunEx("Cannot insert config '") << conf << "': " << sql.lastError() <<
							   raise;
		configs.emplace(std::move(realConf), type);
	});
}

void parseKconfigs(Kconfig::Config::Configs &configs, SQL::F2CSQLConn &sql,
		   const std::filesystem::path &root)
{
	Kconfig::Parser p;
	const auto excludeDir = root / "scripts" / "kconfig" / "tests";
	const auto excludePath = root / "scripts" / "Kconfig.include";

	for (const auto &e: Kconfig::ConfigRange{}) {
		std::string name(Kconfig::Config::getName(e));
		if (!sql.insertConfigType(static_cast<unsigned>(e), name))
			RunEx("Cannot insert config type '") << name << "': " <<
							       sql.lastError() << raise;
	}

	for (auto it = std::filesystem::recursive_directory_iterator(root);
	     it != std::filesystem::end(it); ++it) {
		const auto &path = it->path();
		if (it->is_directory()) {
			if (path == excludeDir)
				it.disable_recursion_pending();
			continue;
		}
		if (!it->is_regular_file())
			continue;
		if (path.stem() != "Kconfig" && path.stem() != "Kconfig-nommu")
			continue;
		if (path == excludePath)
			continue;

		if (!p.parse(path, false))
			RunEx("Cannot parse: ") << path << raise;

		insertConfigSQL(configs, p, sql);
	}
}

void parseKbuilds(const Kconfig::Config::Configs &configs, SQL::F2CSQLConn &sql,
		  const SlKernCVS::SupportedConf &supp, const std::string &branch,
		  const std::filesystem::path &root)
{
	TW::SQLiteMakeVisitor visitor(sql, supp, branch, root, configs);
	TW::TreeWalker tw(root, configs, visitor);
	tw.walk();
}

void processF2C(Kconfig::Config::Configs &configs, SQL::F2CSQLConn &sql,
		const SlKernCVS::SupportedConf &supp, const std::string &branch,
		const std::filesystem::path &root)
{
	parseKconfigs(configs, sql, root);
	parseKbuilds(configs, sql, supp, branch, root);
}

void processAuthors(const Opts &opts, SQL::F2CSQLConn &sql, const std::string &branch,
		    const SlGit::Repo &repo, const SlGit::Commit &commit)
{
	SlKernCVS::PatchesAuthors PA{repo, opts.authorsDumpRefs, opts.authorsReportUnhandled};

	auto ret = PA.processAuthors(commit, [&sql](const std::string &email) -> bool {
		return sql.insertUser(email);
	}, [&branch, &sql](const std::string &email, const std::filesystem::path &path,
			unsigned count, unsigned realCount) -> bool {
		auto fileDir = sql.insertPath(path);
		return fileDir && sql.insertUFMap(branch, email, std::move(fileDir->first),
						   std::move(fileDir->second),
						   count, realCount);
	});
	if (!ret)
		RunEx("Cannot process authors").raise();
}

void processConfigs(SQL::F2CSQLConn &sql, const std::string &branch, const SlGit::Repo &repo,
		    const SlGit::Commit &commit, const Kconfig::Config::Configs &configs)
{
	std::string error;

	SlKernCVS::CollectConfigs CC{repo,
		[&sql, &error](const std::string &arch, const std::string &flavor) {
			auto ret = sql.insertArch(arch) && sql.insertFlavor(flavor);
			if (!ret)
				error = sql.lastError();
			return ret;
		}, [&sql, &branch, &error, &configs](const std::string &arch,
						     const std::string &flavor,
						     const std::string &config,
						     const SlKernCVS::CollectConfigs::ConfigValue &value) {
			if (!configs.contains(config)) {
				Clr(std::cerr, Clr::YELLOW) << "config \"" << config <<
							       "\" is not defined (" << arch <<
							       '/' << flavor << ')';
				return true;
			}
			auto ret = sql.insertCBMap(branch, arch, flavor, config,
						   std::string(1, value));
			if (!ret)
				error = sql.lastError();
			return ret;
	}};

	if (!CC.collectConfigs(commit))
		RunEx("Cannot collect configs: ") << error << raise;
}

void processIgnore(SQL::F2CSQLConn &sql, const std::string &branch,
		   const std::vector<Json> &patterns, const std::filesystem::path &relPath)
{
	for (const auto &pattern: patterns)
		if (!fnmatch(pattern.get_ref<const Json::string_t &>().c_str(),
			     relPath.c_str(), FNM_PATHNAME)) {
			const auto dirFile = sql.insertPath(relPath);
			if (!dirFile || !sql.insertIFBMap(branch, dirFile->first, dirFile->second))
				RunEx("Cannot insert ignore: ") << sql.lastError() << raise;
		}
}

void processIgnores(SQL::F2CSQLConn &sql, const std::string &branch, const Json &json,
		    const std::filesystem::path &root)
{
	if (!json.contains("ignored_files"))
		return;
	const auto ignoredFiles = json["ignored_files"];
	const auto allIt = ignoredFiles.find("all");
	const auto all = (allIt != ignoredFiles.end()) ?
		&allIt->get_ref<const Json::array_t &>() : nullptr;

	const auto forBranchIt = ignoredFiles.find(branch);
	const auto forBranch = (forBranchIt != ignoredFiles.end()) ?
				&forBranchIt->get_ref<const Json::array_t &>() : nullptr;

	for (const auto &e: std::filesystem::recursive_directory_iterator(root)) {
		if (!e.is_regular_file())
			continue;

		const auto relPath = e.path().lexically_relative(root);

		if (all)
			processIgnore(sql, branch, *all, relPath);
		if (forBranch)
			processIgnore(sql, branch, *forBranch, relPath);
	}
}

void processBranch(const Opts &opts, const std::string &branchNote,
		   SQL::F2CSQLConn &sql,
		   const std::string &branch, const SlGit::Repo &repo, SlGit::Commit &commit,
		   const std::filesystem::path &root, const std::optional<Json> &configuration,
		   BranchesProps &branchesProps)
{
	sql.begin();
	auto SHA = commit.idStr();
	BranchProps props{ commit };

	if (!sql.insertBranch(branch, SHA, props.version))
		RunEx("Cannot add branch '") << branch << "' with SHA '" << SHA << '\'' <<
						raise;

	branchesProps.emplace(branch, std::move(props));

	if (!opts.sqliteCreateOnly) {
		Clr(Clr::GREEN) << "== " << branchNote << " -- Retrieving supported info ==";
		auto supp = getSupported(commit);

		Clr(Clr::GREEN) << "== " << branchNote << " -- Running file2config ==";
		Kconfig::Config::Configs configs;
		processF2C(configs, sql, supp, branch, root);

		Clr(Clr::GREEN) << "== " << branchNote << " -- Collecting configs ==";
		processConfigs(sql, branch, repo, commit, configs);

		Clr(Clr::GREEN) << "== " << branchNote <<
				       " -- Detecting authors of patches ==";
		processAuthors(opts, sql, branch, repo, commit);

		if (configuration) {
			Clr(Clr::GREEN) << "== " << branchNote <<
					       " -- Collecting ignored files ==";
			processIgnores(sql, branch, *configuration, root);
		}
	}

	Clr(Clr::GREEN) << "== " << branchNote << " -- Committing ==";
	sql.end();
}

} // namespace

void handleEx(int argc, char **argv)
{
	const auto opts = getOpts(argc, argv);

	auto configuration = loadConfiguration(opts);

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

	auto branchNo = 0U;
	auto branchCnt = branches.size();

	BranchesProps branchesProps;
	for (const auto &branch: branches) {
		auto branchNote = getBranchNote(branch, ++branchNo, branchCnt);
		Clr(Clr::GREEN) << "== " << branchNote << " -- Starting ==";
		if (skipBranch(sql, branch, opts.force)) {
			Clr(Clr::YELLOW) << "Already present, skipping, use -f to force re-creation";
			continue;
		}

		auto branchCommit = checkoutBranch(branchNote, branch, repo);
		auto expandedTree = getExpandedDir(scratchArea, branch);

		expandBranch(branchNote, scratchArea, expandedTree);
		processBranch(opts, branchNote, sql, branch, repo, branchCommit, expandedTree,
			      configuration, branchesProps);
	}

	if (!opts.noRenames) {
		Clr(Clr::GREEN) << "== Collecting renames ==";
		Renames::processRenames(sql, *lrepo, branchesProps);

		if (!sql.exec("VACUUM;"))
			RunEx("Cannot VACUUM the DB: ") << sql.lastError() << raise;
	}
}

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
