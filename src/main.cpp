// SPDX-License-Identifier: GPL-2.0-only

#include <cxxopts.hpp>
#include <iostream>

#include <sl/kerncvs/Branches.h>
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
		("append-branch", "process also this branch",
			cxxopts::value<std::vector<std::string>>())
		("branch", "branch to process",
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
	std::cout << "cmd=" << ss.str() << " sys=" << std::hex << stat << std::dec << '\n';
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

class PatchesAuthors {
public:
	using Map = std::map<std::string, std::map<std::string, unsigned int>>;

	PatchesAuthors(bool dumpRefs, bool reportUnhandled) : dumpRefs(dumpRefs),
		reportUnhandled(reportUnhandled)
	{}

	int processPatch(const std::filesystem::path &file, const std::string &content);

	const Map &HoH() const { return m_HoH; }
	const Map &HoHReal() const { return m_HoHReal; }
	const Map &HoHRefs() const { return m_HoHRefs; }

private:
	const bool dumpRefs;
	const bool reportUnhandled;
	static const std::regex REInteresting;
	static const std::regex REFalse;
	static const std::regex REGitFixes;
	static const std::regex REInvalRef;
	Map m_HoH;
	Map m_HoHReal;
	Map m_HoHRefs;
};

const std::regex PatchesAuthors::REInteresting("^\\s*(?:From|Cc|Co-developed-by|Acked|Acked-by|Modified-by|Reviewed-by|Reviewed-and-tested-by|Signed-off-by):.*[\\s<]([a-z0-9_.-]+\\@suse\\.[a-z]+)",
	      std::regex_constants::icase);
const std::regex PatchesAuthors::REFalse("(?:lore|lkml)\\.kernel|patchwork\\.ozlabs|^\\[|^(?:Debugged-by|Evaluated-by|Improvements-by|Link|Message-ID|Patch-mainline|Reported-and-tested-by|Reported-by|Return-path|Suggested-by|Tested-by):|thanks|:$",
	      std::regex_constants::icase);
const std::regex PatchesAuthors::REGitFixes("^References:.*(?:(?:git|stable)[- ]fixes|stable-\\d|b[ns]c[#](?:1012628|1051510|1151927|1152489))",
	   std::regex_constants::icase);
const std::regex PatchesAuthors::REInvalRef("FATE#|CVE-|jsc#|XSA-", std::regex_constants::icase);

int PatchesAuthors::processPatch(const std::filesystem::path &file, const std::string &content)
{
	if (F2C::verbose > 2)
		std::cout << __func__ << ": " << file << '\n';
	std::set<std::string> patchEmails;
	std::set<std::string> patchRefs;
	bool gitFixes = false;
	std::istringstream iss(content);
	std::string line;
	std::smatch m;

	while (std::getline(iss, line)) {
		if (std::regex_search(line, m, REInteresting)) {
			patchEmails.insert(m[1]);
			continue;
		}
		if (SlHelpers::String::startsWith(line, "---"))
			break;
		if (std::regex_search(line, REGitFixes)) {
			gitFixes = true;
		} else if (dumpRefs) {
			static const std::string references { "References:" };
			if (SlHelpers::String::startsWith(line, references))
				for (const auto &ref: SlHelpers::String::split(line.substr(references.size()),
									" \t,;"))
					patchRefs.insert(ref);
		}

		if (reportUnhandled && line.find("@suse.") != std::string::npos &&
				!std::regex_search(line, REFalse))
			std::cerr << file << ": unhandled e-mail in '" << line << "'\n";
	}

	for (const auto &ref : patchRefs)
		for (const auto &email : patchEmails)
			if (!std::regex_search(ref, REInvalRef))
				m_HoHRefs[email][ref]++;

	while (std::getline(iss, line)) {
		static const std::string prefix { "+++ b/" };
		if (!SlHelpers::String::startsWith(line, prefix))
			continue;
		if (!SlHelpers::String::endsWith(line, ".c") &&
				!SlHelpers::String::endsWith(line, ".h"))
			continue;

		auto cfile = line.substr(prefix.length());
		if (SlHelpers::String::startsWith(cfile, "/dev"))
			std::cerr << __func__ << ": " << file << ": " << cfile << '\n';
		for (const auto &email : patchEmails) {
			m_HoH[email][cfile]++;
			if (gitFixes)
				m_HoHReal[email][cfile]; // add so it exists
			else
				m_HoHReal[email][cfile]++;
		}
	}

	return 0;
}

static int processAuthors(const std::unique_ptr<SQL::F2CSQLConn> &sql, const std::string &branch,
			  const SlGit::Repo &repo, const SlGit::Commit &commit, bool dumpRefs,
			  bool reportUnhandled)
{
	if (!sql)
		return 0;

	PatchesAuthors PA(dumpRefs, reportUnhandled);

	SlGit::Tree tree;
	tree.ofCommit(commit);

	SlGit::TreeEntry patchesSuseTreeEntry;
	if (patchesSuseTreeEntry.byPath(tree, "patches.suse/"))
		return -1;
	if (patchesSuseTreeEntry.type() != GIT_OBJECT_TREE)
		return -1;

	SlGit::Tree patchesSuseTree;
	if (patchesSuseTree.lookup(repo, patchesSuseTreeEntry))
		return -1;
	auto ret = patchesSuseTree.walk([&repo, &PA](const std::string &root,
					const SlGit::TreeEntry &entry) -> int {
		SlGit::Blob blob;
		if (blob.lookup(repo, entry))
			return -1000;

		return PA.processPatch(root + entry.name(), blob.content());
	});
	if (ret)
		return -1;

	for (const auto &pair : PA.HoHRefs())
		for (const auto &refPair : pair.second)
			if (refPair.second > 100) {
				std::cout << std::setw(30) << pair.first <<
					     std::setw(40) << refPair.first <<
					     std::setw(5) << refPair.second << '\n';
			}

	for (const auto &pair : PA.HoH()) {
		const auto &email = pair.first;
		const auto &realMap = PA.HoHReal().at(email);
		if (sql->insertUser(email))
			return -1;

		for (const auto &pairSrc : pair.second) {
			std::filesystem::path path(pairSrc.first);

			if (sql->insertUFMap(branch, email, path.parent_path().string(),
					     path.filename().string(), pairSrc.second,
					     realMap.at(pairSrc.first)))
				return -1;
		}
	}

	return 0;
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

		std::cout << "== " << branchNote << " -- Detecting authors of patches ==\n";//, 'green');
		processAuthors(sql, branch, repo, commit, dumpRefs, reportUnhandled);
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
