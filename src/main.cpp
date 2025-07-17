// SPDX-License-Identifier: GPL-2.0-only

#include <cxxopts.hpp>
#include <iostream>

#include <sl/kerncvs/Branches.h>
#include <sl/kerncvs/CollectConfigs.h>
#include <sl/kerncvs/PatchesAuthors.h>
#include <sl/kerncvs/SupportedConf.h>
#include <sl/git/Git.h>
#include <sl/helpers/PushD.h>
#include <sl/helpers/String.h>

#include "sql/F2CSQLConn.h"
#include "treewalker/ConsoleMakeVisitor.h"
#include "treewalker/SQLiteMakeVisitor.h"
#include "treewalker/TreeWalker.h"
#include "Verbose.h"

static cxxopts::ParseResult getOpts(int argc, char **argv)
{
	cxxopts::Options options { argv[0], "Generate conf_file_map database (and more)" };
	options.add_options()
		("a,append-branch", "process also this branch",
			cxxopts::value<std::vector<std::string>>())
		("b,branch", "branch to process",
			cxxopts::value<std::vector<std::string>>())
		("dest", "destination (scratch area)",
			cxxopts::value<std::filesystem::path>()->default_value("fill-db"))
		("f,force", "force branch creation (delete old data)")
		("q,quiet", "quiet mode")
		("v,verbose", "verbose mode")
	;
	options.add_options("authors")
		("authors-dump-refs", "dump references to stdout")
		("authors-report-unhandled", "report unhandled lines to stdout")
	;
	options.add_options("sqlite")
		("s,sqlite", "create db",
			cxxopts::value<std::filesystem::path>()->
			implicit_value("conf_file_map.sqlite"))
		("S,sqlite-create", "create the db if not exists")
		("O,sqlite-create-only", "only create the db (do not fill it)")
	;

	try {
		return options.parse(argc, argv);
	} catch (const cxxopts::exceptions::parsing &e) {
		std::cerr << "arguments error: " << e.what() << '\n';
		std::cerr << options.help();
		exit(EXIT_FAILURE);
	}
}

static std::optional<std::filesystem::path> prepareScratchArea(const cxxopts::ParseResult &opts)
{
	std::filesystem::path scratchArea;
	if (opts.contains("dest")) {
		scratchArea = opts["dest"].as<std::filesystem::path>();
	} else if (auto scratchAreaEnv = std::getenv("SCRATCH_AREA")) {
		scratchArea = scratchAreaEnv;
		scratchArea /= "fill-db";
	} else {
		std::cerr << "Neither --dest, nor SCRATCH_AREA defined (defaulting to \"fill-db\")\n";
		scratchArea = "fill-db";
	}
	std::error_code ec;
	std::filesystem::create_directories(scratchArea, ec);
	if (ec) {
		std::cerr << __func__ << ": cannot create " << scratchArea << ": error=" << ec << '\n';
		return {};
	}

	return std::filesystem::absolute(scratchArea);
}

static int prepareKsourceGit(const std::filesystem::path &scratchArea, SlGit::Repo &repo)
{
	static const std::string kerncvs { "jslaby@kerncvs.suse.de:/srv/git/kernel-source.git" };

	auto ourKsourceGit = scratchArea / "kernel-source";

	if (std::filesystem::exists(ourKsourceGit))
		return repo.open(ourKsourceGit);

	int ret = repo.init(ourKsourceGit, false, kerncvs);
	if (ret) {
		std::cerr << __func__ << ": cannot init: " << git_error_last()->message << '\n';
		return ret;
	}

	SlGit::Remote origin;
	origin.lookup(repo, "origin");
	ret = origin.fetch("scripts", 1, false);
	if (ret) {
		std::cerr << __func__ << ": cannot fetch: " << git_error_last()->message << '\n';
		return ret;
	}

	SlGit::Reference refOrigin;
	ret = refOrigin.lookup(repo, "refs/remotes/origin/scripts");
	if (ret) {
		std::cerr << __func__ << ": cannot obtain ref of origin: " <<
			     git_error_last()->message << '\n';
		return ret;
	}

	SlGit::Reference ref;
	ret = ref.createDirect(repo, "refs/heads/scripts", *refOrigin.target());
	if (ret) {
		std::cerr << __func__ << ": cannot create branch: " << git_error_last()->message <<
			     '\n';
		return ret;
	}

	ret = repo.checkout("refs/heads/scripts");
	if (ret) {
		std::cerr << __func__ << ": cannot checkout: " << git_error_last()->message << '\n';
		return ret;
	}

	std::error_code ec;
	SlHelpers::PushD push(ourKsourceGit, ec);
	if (ec)
		return ec.value();

	auto stat = std::system("./scripts/install-git-hooks");
	if (stat) {
		std::cerr << __func__ << ": cannot install hooks: " << WEXITSTATUS(stat) << '\n';
		return stat;
	}

	return 0;
}

static std::unique_ptr<SQL::F2CSQLConn> getSQL(bool sqlite, const std::filesystem::path &DBPath,
					       bool createDB, bool skipWalk)
{
	if (!sqlite)
		return {};

	auto sql = std::make_unique<SQL::F2CSQLConn>();
	unsigned openFlags = 0;
	if (createDB)
		openFlags |= SlSqlite::CREATE;
	int ret = sql->openDB(DBPath, openFlags);
	if (ret)
		return {};
	if (createDB) {
		ret = sql->createDB();
		if (ret)
			return {};
	}
	if (!skipWalk) {
		ret = sql->prepDB();
		if (ret)
			return {};
	}

	return sql;
}

static std::string getBranchNote(const std::string &branch, const unsigned &branchNo,
				 const unsigned &branchCnt)
{
	auto percent = 100.0 * branchNo / branchCnt;
	std::stringstream ss;
	ss << branch << " (" << branchNo << "/" << branchCnt << " -- " <<
	      std::fixed << std::setprecision(2) << percent << " %)";
	return ss.str();
}

static std::optional<bool> skipBranch(const std::unique_ptr<SQL::F2CSQLConn> &sql,
				      const std::string &branch, bool force)
{
	if (!sql)
		return false;

	if (force) {
		if (sql->deleteBranch(branch))
			return {};
		return false;
	}

	return sql->hasBranch(branch);
}

static int checkoutBranch(const std::string &branchNote, const std::string &branch,
			  SlGit::Repo &repo, SlGit::Commit &commit)
{
	std::cout << "== " << branchNote << " -- Checking Out ==\n";//, 'green');
	if (repo.checkout("refs/remotes/origin/" + branch))
		return -1;
	if (commit.revparseSingle(repo, "HEAD"))
		return -1;
	return 0;
}

static std::filesystem::path getExpandedDir(const std::filesystem::path &scratchArea,
					    const std::string &branch)
{
	auto branchDir(branch);
	std::replace(branchDir.begin(), branchDir.end(), '/', '_');
	return scratchArea / branchDir;
}

static int expandBranch(const std::string &branchNote, const std::filesystem::path &scratchArea,
			const std::filesystem::path &expandedTree)
{
	auto kernelSource = scratchArea / "kernel-source";
	std::error_code ec;
	SlHelpers::PushD push(kernelSource, ec);
	if (ec) {
		std::cerr << __func__ << ": cannot chdir to " << kernelSource << '\n';
		return -1;
	}

	std::cout << "== " << branchNote << " -- Expanding ==\n";//, 'green');
	std::stringstream ss;
	if (std::filesystem::exists("./scripts/sequence-patch"))
		ss << "./scripts/sequence-patch";
	else // temporary for old branches
		ss << "./scripts/sequence-patch.sh";
	ss << " --dir='" << scratchArea.string() << "'";
	ss << " --patch-dir='" << expandedTree.string() << "'";
	ss << " --rapid";
	auto stat = std::system(ss.str().c_str());
	if (F2C::verbose > 1)
		std::cout << "cmd=" << ss.str() << " sys=0x" << std::hex << stat << std::dec << '\n';
	if (stat) {
		std::cerr << __func__ << ": cannot seq patch: " << WEXITSTATUS(stat) << '\n';
		return -1;
	}

	return 0;
}

static std::unique_ptr<TW::MakeVisitor> getMakeVisitor(const std::unique_ptr<SQL::F2CSQLConn> &sql,
						       const SlKernCVS::SupportedConf &supp,
						       const std::string &branch,
						       const std::filesystem::path &root)
{
	if (sql)
		return std::make_unique<TW::SQLiteMakeVisitor>(*sql, supp, branch, root);
	else
		return std::make_unique<TW::ConsoleMakeVisitor>();
}

static std::optional<SlKernCVS::SupportedConf> getSupported(const SlGit::Repo &repo,
							    const SlGit::Commit &commit)
{
	auto suppConf = commit.catFile(repo, "supported.conf");
	if (!suppConf)
		return {};

	return SlKernCVS::SupportedConf { *suppConf };
}

static int processAuthors(const std::unique_ptr<SQL::F2CSQLConn> &sql, const std::string &branch,
			  const SlGit::Repo &repo, const SlGit::Commit &commit, bool dumpRefs,
			  bool reportUnhandled)
{
	SlKernCVS::PatchesAuthors PA{repo, dumpRefs, reportUnhandled};

	return PA.processAuthors(commit, [&sql](const std::string &email) -> int {
		return sql->insertUser(email);
	}, [&branch, &sql](const std::string &email, const std::filesystem::path &path,
			unsigned count, unsigned realCount) -> int {
		return sql->insertUFMap(branch, email, path.parent_path().string(),
				     path.filename().string(), count, realCount);
	});

	return 0;
}

static int processConfigs(const std::unique_ptr<SQL::F2CSQLConn> &sql, const std::string &branch,
			  const SlGit::Repo &repo, const SlGit::Commit &commit)
{
	SlKernCVS::CollectConfigs CC{repo,
		[&sql](const std::string &arch, const std::string &flavor) -> int {
			if (sql->insertArch(arch))
				return -1;
			return sql->insertFlavor(flavor);
		}, [&sql, &branch](const std::string &arch, const std::string &flavor,
		      const std::string &config,
		      const SlKernCVS::CollectConfigs::ConfigValue &value) -> int {
			if (sql->insertConfig(config))
				return -1;
			return sql->insertCBMap(branch, arch, flavor, config,
						std::string(1, value));
	}};

	return CC.collectConfigs(commit);
}

int processBranch(const std::string &branchNote, const std::unique_ptr<SQL::F2CSQLConn> &sql,
		  const std::string &branch, const SlGit::Repo &repo, SlGit::Commit &commit,
		  bool skipWalk, const std::filesystem::path &root, bool dumpRefs,
		  bool reportUnhandled)
{
	if (sql) {
		sql->begin();
		auto SHA = commit.idStr();
		if (sql->insertBranch(branch, SHA)) {
			std::cerr << "cannot add branch '" << branch <<
				     "' with SHA '" << SHA << "'\n";
			return -1;
		}
	}

	if (!skipWalk) {
		std::cout << "== " << branchNote << " -- Retrieving supported info ==\n";//, 'green');
		auto supp = getSupported(repo, commit);
		if (!supp)
			return -1;

		std::cout << "== " << branchNote << " -- Running file2config ==\n";//, 'green');
		auto visitor = getMakeVisitor(sql, *supp, branch, root);
		TW::TreeWalker tw(root, *visitor);
		tw.walk();

		if (sql) {
			std::cout << "== " << branchNote << " -- Collecting configs ==\n";//, 'green');
			if (processConfigs(sql, branch, repo, commit))
				return -1;

			std::cout << "== " << branchNote << " -- Detecting authors of patches ==\n";//, 'green');
			if (processAuthors(sql, branch, repo, commit, dumpRefs, reportUnhandled))
				return -1;
		}
	}

	if (sql) {
		std::cout << "== " << branchNote << " -- Committing ==\n";//, 'green');
		sql->end();
	}

	return 0;
}

int main(int argc, char **argv)
{
	auto opts = getOpts(argc, argv);

	F2C::quiet = opts.contains("quiet");
	F2C::verbose = opts.count("verbose");

	auto scratchArea = prepareScratchArea(opts);
	if (!scratchArea)
		return EXIT_FAILURE;

	SlGit::Repo repo;

	if (prepareKsourceGit(*scratchArea, repo))
		return EXIT_FAILURE;

	SlKernCVS::Branches::BranchesList branches;
	if (opts.contains("branch")) {
		branches = opts["branch"].as<std::vector<std::string>>();
	} else {
		auto branchesOpt = SlKernCVS::Branches::getBuildBranches();
		if (!branchesOpt)
			return EXIT_FAILURE;
		branches = *branchesOpt;
	}

	if (auto append = opts["append-branch"].as_optional<std::vector<std::string>>())
		branches.insert(branches.end(), append->begin(), append->end());

	SlGit::Remote remote;
	if (remote.lookup(repo, "origin"))
		return EXIT_FAILURE;
	if (remote.fetchBranches(branches, 1, false)) {
		auto lastErr = git_error_last();
		std::cerr << "fetch failed: " << lastErr->message <<
			     " (" << lastErr->klass << ")\n";
		return EXIT_FAILURE;
	}

	auto sqlite = opts.contains("sqlite");
	auto sqliteDB = sqlite ? opts["sqlite"].as<std::filesystem::path>() : "";
	auto skipWalk = opts.contains("sqlite-create-only");
	auto sql = getSQL(sqlite, sqliteDB, opts.contains("sqlite-create"), skipWalk);
	if (sqlite && !sql)
		return EXIT_FAILURE;

	auto branchNo = 0U;
	auto branchCnt = branches.size();
	auto forceCreate = opts.contains("force");
	auto dumpRefs = opts.contains("authors-dump-refs");
	auto reportUnhandled = opts.contains("authors-report-unhandled");

	for (const auto &branch: branches) {
		auto branchNote = getBranchNote(branch, ++branchNo, branchCnt);
		std::cout << "== " << branchNote << " -- Starting ==\n";//, 'green');
		auto skipOpt = skipBranch(sql, branch, forceCreate);
		if (!skipOpt)
			return EXIT_FAILURE;
		if (*skipOpt) {
			std::cout << "Already present, skipping, use -f to force re-creation\n";//, 'yellow');
			continue;
		}

		SlGit::Commit branchCommit;
		if (checkoutBranch(branchNote, branch, repo, branchCommit))
			return EXIT_FAILURE;

		auto expandedTree = getExpandedDir(*scratchArea, branch);

		if (expandBranch(branchNote, *scratchArea, expandedTree))
			return EXIT_FAILURE;

		if (processBranch(branchNote, sql, branch, repo, branchCommit, skipWalk,
				  expandedTree, dumpRefs, reportUnhandled))
			return EXIT_FAILURE;
	}

	return 0;
}
