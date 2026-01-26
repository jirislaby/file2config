// SPDX-License-Identifier: GPL-2.0-only

#include <cxxopts.hpp>
#include <fnmatch.h>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

#include <sl/kerncvs/Branches.h>
#include <sl/kerncvs/CollectConfigs.h>
#include <sl/kerncvs/PatchesAuthors.h>
#include <sl/kerncvs/RPMConfig.h>
#include <sl/kerncvs/SupportedConf.h>
#include <sl/git/Git.h>
#include <sl/helpers/Color.h>
#include <sl/helpers/Misc.h>
#include <sl/helpers/Process.h>
#include <sl/helpers/PtrStore.h>
#include <sl/helpers/PushD.h>
#include <sl/helpers/String.h>

#include "sql/F2CSQLConn.h"
#include "treewalker/ConsoleMakeVisitor.h"
#include "treewalker/SQLiteMakeVisitor.h"
#include "treewalker/TreeWalker.h"
#include "Verbose.h"

using Clr = SlHelpers::Color;
using Json = nlohmann::ordered_json;

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

	std::filesystem::path ignoredFilesJSON;
	bool hasIgnoredFiles;

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
	options.add_options("files")
		("ignored-files", "path to JSON containing files to be added to ignore table",
			cxxopts::value(opts.ignoredFilesJSON))
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
		opts.hasIgnoredFiles = cxxopts.contains("ignored-files");
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
						SlGit::Repo::lastError();
		return std::nullopt;
	}

	auto origin = repo->remoteLookup("origin");
	auto ret = origin->fetch("scripts", 1, false);
	if (ret) {
		Clr(std::cerr, Clr::RED) << __func__ << ": cannot fetch: " <<
						repo->lastError();
		return std::nullopt;
	}

	ret = repo->checkout("refs/remotes/origin/scripts");
	if (ret) {
		Clr(std::cerr, Clr::RED) << __func__ << ": cannot checkout: " <<
						repo->lastError();
		return std::nullopt;
	}

	std::error_code ec;
	SlHelpers::PushD push(ourKsourceGit, ec);
	if (ec)
		return std::nullopt;

	SlHelpers::Process P;
	if (!P.run("./scripts/install-git-hooks") || P.exitStatus()) {
		Clr(std::cerr, Clr::RED) << __func__ << ": cannot install hooks: " <<
					    P.lastError() << " (" << P.exitStatus() << ')';
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
	if (!sql->openDB(opts.sqlite, openFlags)) {
		Clr(std::cerr, Clr::RED) << "cannot open/create the db at " << opts.sqlite << ": "
					 << sql->lastError();
		return {};
	}
	if (opts.sqliteCreate) {
		if (!sql->createDB()) {
			Clr(std::cerr, Clr::RED) << "cannot create tables: " << sql->lastError();
			return {};
		}
	}
	if (!opts.sqliteCreateOnly) {
		if (!sql->prepDB()) {
			Clr(std::cerr, Clr::RED) << "cannot prepare statements: " <<
						    sql->lastError();
			return {};
		}
	}

	return sql;
}

std::optional<Json> loadIgnoredFiles(const Opts &opts)
{
	if (!opts.hasIgnoredFiles)
		return std::nullopt;

	std::ifstream ifs{opts.ignoredFilesJSON};
	if (!ifs) {
		Clr(std::cerr, Clr::RED) << "cannot open JSON " << opts.ignoredFilesJSON;
		return std::nullopt;
	}
	Json json;
	try {
		json = json.parse(ifs);
	} catch (const Json::exception &e) {
		Clr(std::cerr, Clr::RED) << "cannot parse JSON from " << opts.ignoredFilesJSON <<
					    ": " << e.what();
		return std::nullopt;
	}

	return json;
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
	if (!repo.checkout("refs/remotes/origin/" + branch)) {
		Clr(std::cerr, Clr::RED) << "Cannot check out '" << branch << "': " <<
					    repo.lastError();
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
	if (!ret || P.exitStatus()) {
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

std::optional<SlKernCVS::SupportedConf> getSupported(const SlGit::Commit &commit)
{
	auto suppConf = commit.catFile("supported.conf");
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
		auto fileDir = sql->insertPath(path);
		return fileDir && sql->insertUFMap(branch, email, std::move(fileDir->first),
						   std::move(fileDir->second),
						   count, realCount);
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

bool processIgnore(const std::unique_ptr<SQL::F2CSQLConn> &sql, const std::string &branch,
		   const std::vector<Json> &patterns, const std::filesystem::path &relPath)
{
	for (const auto &pattern: patterns)
		if (!fnmatch(pattern.get_ref<const Json::string_t &>().c_str(),
			     relPath.c_str(), FNM_PATHNAME)) {
			const auto dir = relPath.parent_path();
			const auto file = relPath.filename();

			if (!sql->insertDir(dir))
				return false;
			if (!sql->insertFile(dir, file))
				return false;
			if (!sql->insertIFBMap(branch, dir, file))
				return false;
		}

	return true;
}
bool processIgnores(const std::unique_ptr<SQL::F2CSQLConn> &sql, const std::string &branch,
		    const Json &json, const std::filesystem::path &root)
{
	const auto allIt = json.find("all");
	const auto all = (allIt != json.end()) ? &allIt->get_ref<const Json::array_t &>() : nullptr;

	const auto forBranchIt = json.find(branch);
	const auto forBranch = (forBranchIt != json.end()) ?
				&forBranchIt->get_ref<const Json::array_t &>() : nullptr;

	for (const auto &e: std::filesystem::recursive_directory_iterator(root)) {
		if (!e.is_regular_file())
			continue;

		const auto relPath = e.path().lexically_relative(root);

		if (all && !processIgnore(sql, branch, *all, relPath))
			return false;
		if (forBranch && !processIgnore(sql, branch, *forBranch, relPath))
			return false;
	}

	return true;
}

bool processBranch(const Opts &opts, const std::string &branchNote,
		   const std::unique_ptr<SQL::F2CSQLConn> &sql,
		   const std::string &branch, const SlGit::Repo &repo, SlGit::Commit &commit,
		   const std::filesystem::path &root, const Json &ignoredFiles)
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
		auto supp = getSupported(commit);
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

			Clr(Clr::GREEN) << "== " << branchNote <<
					       " -- Collecting ignored files ==";
			if (!processIgnores(sql, branch, ignoredFiles, root))
				return false;
		}
	}

	if (sql) {
		Clr(Clr::GREEN) << "== " << branchNote << " -- Committing ==";
		sql->end();
	}

	return true;
}

auto getTagsFromKsourceTree(const SlKernCVS::Branches::BranchesList &branches,
			    const SlGit::Repo &repo)
{
	std::set<std::string, SlHelpers::CmpVersions> ret;

	for (const auto &b: branches) {
		auto rpmConf = SlKernCVS::RPMConfig::create(repo, b);
		if (!rpmConf) {
			Clr(std::cerr, Clr::RED) << "cannot obtain a config for " <<
						    std::quoted(b) << ": " << repo.lastError();
			continue;
		}
		auto srcVer = rpmConf->get("SRCVERSION");
		if (!srcVer) {
			Clr(std::cerr, Clr::RED) << "no SRCVERSION in " << std::quoted(b);
			continue;
		}
		ret.emplace(srcVer->get());
	}

	return ret;
}

struct RenameInfo {
    std::string path;
    unsigned similarity;
};
using RenameMap = std::unordered_map<std::string, RenameInfo, SlHelpers::String::Hash,
		SlHelpers::String::Eq>;

void processRenamesBetween(const std::unique_ptr<SQL::F2CSQLConn> &sql, const SlGit::Repo &lrepo,
			   std::string_view begin, std::string_view end, RenameMap &renames)
{
	auto begVersion = SlHelpers::Version::versionSum(begin);
	std::ostringstream range;
	range << 'v' << begin << "..";
	if (end.empty())
		range << "origin/master";
	else
		range << 'v' << end;

	Clr() << '\t' << range.str();

	// libgit2 is *very* slow at comparing trees, we have to call git log.
	SlHelpers::Process p;
	p.spawn("/usr/bin/git", { "-C", lrepo.workDir(), "log", "-M30", "-l0", "--oneline",
				  "--no-merges", "--raw", "--diff-filter=R",
				  "--format=", range.str() }, true);

	SlHelpers::PtrStore<FILE, decltype([](FILE *f) { if (f) fclose(f); })> stream;
	stream.reset(fdopen(p.readPipe(), "r"));
	if (!stream)
		throw std::runtime_error("cannot open stdout of git");

	SlHelpers::PtrStore<char, decltype([](char *ptr) { free(ptr); })> lineRaw;
	size_t len = 0;

	while (getline(lineRaw.ptr(), &len, stream.get()) != -1) {
		auto line = lineRaw.str();
		if (line.empty() || line.front() != ':')
			throw std::runtime_error("bad line: " + std::string(line));

		auto vec = SlHelpers::String::splitSV(line, " \t\n");
		if (vec.size() < 7)
			throw std::runtime_error("bad formatted line: " + std::string(line));

		unsigned int similarity{};
		std::from_chars(vec[4].data() + 1, vec[4].data() + vec[4].size(), similarity);
		if (!similarity)
			throw std::runtime_error("bad rename part: " + std::string(vec[4]));
		auto oldFile = vec[5];
		auto newFile = vec[6];

		auto it = renames.find(newFile);
		if (it != renames.end()) {
			auto final = std::move(it->second);
			renames.erase(it);

			// do not store reverted and back and forth renames
			if (oldFile != final.path) {
				final.similarity *= similarity;
				final.similarity /= 100U;
				renames.emplace(oldFile, std::move(final));
			}
		} else {
			renames.emplace(oldFile, RenameInfo{std::string(newFile), similarity});
		}
	}

	if (!feof(stream.get()) || ferror(stream.get()))
		throw std::runtime_error(std::string("not completely read: ") + strerror(errno));

	if (!p.waitForFinished())
		throw std::runtime_error("cannot wait for git: " + p.lastError());

	if (p.signalled())
		throw std::runtime_error("git crashed");
	if (auto e = p.exitStatus())
		throw std::runtime_error("git exited with " + std::to_string(e));

	auto trans = sql->beginAuto();
	for (const auto &e: renames) {
		auto oldP = sql->insertPath(e.first);
		if (!oldP)
			throw std::runtime_error("cannot insert old path: " + e.first + ": " +
						 sql->lastError());
		auto newP = sql->insertPath(e.second.path);
		if (!newP)
			throw std::runtime_error("cannot insert new path: " + e.second.path + ": " +
						 sql->lastError());
		if (!sql->insertRFVMap(begVersion, e.second.similarity, oldP->first, oldP->second,
				       newP->first, newP->second))
			throw std::runtime_error("cannot insert rename file map: " + e.first +
						 " -> " + e.second.path + ": " + sql->lastError());
	}
}

bool processRenames(const std::unique_ptr<SQL::F2CSQLConn> &sql,
		    const SlGit::Repo &lrepo, const SlGit::Repo &repo,
		    SlKernCVS::Branches::BranchesList branches)
{
	try {
		auto uniqTags = getTagsFromKsourceTree(branches, repo);
		RenameMap map;
		if (uniqTags.size() >= 1) {
			auto curr = uniqTags.rbegin();

			processRenamesBetween(sql, lrepo, *curr, "", map);
			for (auto prev = std::next(curr); prev != uniqTags.rend(); ++curr, ++prev)
				processRenamesBetween(sql, lrepo, *prev, *curr, map);
		}
	} catch (const std::runtime_error &e) {
		Clr(Clr::RED) << e.what();
		return false;
	}
	return true;
}

} // namespace

int main(int argc, char **argv)
{
	const auto opts = getOpts(argc, argv);

	const auto lpath = SlHelpers::Env::get<std::filesystem::path>("LINUX_GIT");
	if (!lpath) {
		Clr(std::cerr, Clr::RED) << "LINUX_GIT not set";
		return EXIT_FAILURE;
	}

	auto lrepo = SlGit::Repo::open(*lpath);
	if (!lrepo) {
		Clr(std::cerr, Clr::RED) << "Cannot open LINUX_GIT repo: " <<
					    SlGit::Repo::lastError() <<
					    " (" << SlGit::Repo::lastClass() << ')';
		return EXIT_FAILURE;
	}

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
		if (!branchesOpt) {
			Clr(std::cerr, Clr::RED) << "Cannot download branches.conf";
			return EXIT_FAILURE;
		}
		branches = *branchesOpt;
	}

	branches.insert(branches.end(), opts.appendBranches.begin(), opts.appendBranches.end());

	Clr(Clr::GREEN) << "== Fetching branches ==";

	auto remote = repo->remoteLookup("origin");
	if (!remote)
		return EXIT_FAILURE;
	if (!remote->fetchBranches(branches, 1, false)) {
		Clr(std::cerr, Clr::RED) << "Fetch failed: " << repo->lastError() <<
						" (" << repo->lastClass() << ')';
		return EXIT_FAILURE;
	}

	auto sql = opts.hasSqlite ? getSQL(opts) : nullptr;
	if (opts.hasSqlite && !sql)
		return EXIT_FAILURE;

	auto ignoredFiles = loadIgnoredFiles(opts);
	if (opts.hasIgnoredFiles && !ignoredFiles)
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
				   expandedTree, *ignoredFiles))
			return EXIT_FAILURE;
	}

	if (sql) {
		Clr(Clr::GREEN) << "== Collecting renames ==";
		if (!processRenames(sql, *lrepo, *repo, branches))
			return EXIT_FAILURE;

		if (!sql->exec("VACUUM;")) {
			Clr(Clr::RED) << "Cannot VACUUM the DB";
			return EXIT_FAILURE;
		}
	}

	return 0;
}
