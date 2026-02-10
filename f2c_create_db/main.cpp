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
#include <sl/helpers/Exception.h>
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
using RunEx = SlHelpers::RuntimeException;
using SlHelpers::raise;

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
		Clr::forceColorValue(cxxopts.contains("force-color"));
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

std::optional<SQL::F2CSQLConn> getSQL(const Opts &opts)
{
	if (!opts.hasSqlite)
		return std::nullopt;

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

std::optional<Json> loadIgnoredFiles(const Opts &opts)
{
	if (!opts.hasIgnoredFiles)
		return std::nullopt;

	std::ifstream ifs{opts.ignoredFilesJSON};
	if (!ifs)
		RunEx("Cannot open JSON: ") << opts.ignoredFilesJSON << raise;

	Json json;
	try {
		json = json.parse(ifs);
	} catch (const Json::exception &e) {
		RunEx("Cannot parse JSON from ") << opts.ignoredFilesJSON << ": " << e.what() <<
						    raise;
	}

	return json;
}

std::string getBranchNote(const std::string &branch, const unsigned &branchNo,
			  const unsigned &branchCnt)
{
	auto percent = 100.0 * branchNo / branchCnt;
	std::ostringstream ss;
	ss << branch << " (" << branchNo << "/" << branchCnt << " -- " <<
	      std::fixed << std::setprecision(2) << percent << " %)";
	return ss.str();
}

bool skipBranch(std::optional<SQL::F2CSQLConn> &sql, const std::string &branch, bool force)
{
	if (!sql)
		return false;

	if (force) {
		if (!sql->deleteBranch(branch))
			RunEx("Cannot delete branch '") << branch << "': " << sql->lastError() <<
							  raise;
		return false;
	}

	return sql->hasBranch(branch);
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

std::filesystem::path getExpandedDir(const std::filesystem::path &scratchArea,
				     const std::string &branch)
{
	auto branchDir(branch);
	std::replace(branchDir.begin(), branchDir.end(), '/', '_');
	return scratchArea / branchDir;
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

std::unique_ptr<TW::MakeVisitor> getMakeVisitor(std::optional<SQL::F2CSQLConn> &sql,
						const SlKernCVS::SupportedConf &supp,
						const std::string &branch,
						const std::filesystem::path &root)
{
	if (sql)
		return std::make_unique<TW::SQLiteMakeVisitor>(*sql, supp, branch, root);
	else
		return std::make_unique<TW::ConsoleMakeVisitor>();
}

SlKernCVS::SupportedConf getSupported(const SlGit::Commit &commit)
{
	auto suppConf = commit.catFile("supported.conf");
	if (!suppConf)
		RunEx("Cannot obtain supported.conf: ") << commit.repo().lastError() << raise;

	return SlKernCVS::SupportedConf { *suppConf };
}

void processAuthors(const Opts &opts, std::optional<SQL::F2CSQLConn> &sql,
		    const std::string &branch, const SlGit::Repo &repo, const SlGit::Commit &commit)
{
	SlKernCVS::PatchesAuthors PA{repo, opts.authorsDumpRefs, opts.authorsReportUnhandled};

	auto ret = PA.processAuthors(commit, [&sql](const std::string &email) -> bool {
		return sql->insertUser(email);
	}, [&branch, &sql](const std::string &email, const std::filesystem::path &path,
			unsigned count, unsigned realCount) -> bool {
		auto fileDir = sql->insertPath(path);
		return fileDir && sql->insertUFMap(branch, email, std::move(fileDir->first),
						   std::move(fileDir->second),
						   count, realCount);
	});
	if (!ret)
		RunEx("Cannot process authors").raise();
}

void processConfigs(std::optional<SQL::F2CSQLConn> &sql, const std::string &branch,
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

	if (!CC.collectConfigs(commit))
		RunEx("Cannot collect configs").raise();
}

void processIgnore(std::optional<SQL::F2CSQLConn> &sql, const std::string &branch,
		   const std::vector<Json> &patterns, const std::filesystem::path &relPath)
{
	for (const auto &pattern: patterns)
		if (!fnmatch(pattern.get_ref<const Json::string_t &>().c_str(),
			     relPath.c_str(), FNM_PATHNAME)) {
			const auto dir = relPath.parent_path();
			const auto file = relPath.filename();

			if (!sql->insertDir(dir) || !sql->insertFile(dir, file) ||
					!sql->insertIFBMap(branch, dir, file))
				RunEx("Cannot insert ignore: ") << sql->lastError() << raise;
		}
}

void processIgnores(std::optional<SQL::F2CSQLConn> &sql, const std::string &branch,
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

		if (all)
			processIgnore(sql, branch, *all, relPath);
		if (forBranch)
			processIgnore(sql, branch, *forBranch, relPath);
	}
}

void processBranch(const Opts &opts, const std::string &branchNote,
		   std::optional<SQL::F2CSQLConn> &sql,
		   const std::string &branch, const SlGit::Repo &repo, SlGit::Commit &commit,
		   const std::filesystem::path &root, const std::optional<Json> &ignoredFiles)
{
	if (sql) {
		sql->begin();
		auto SHA = commit.idStr();
		if (!sql->insertBranch(branch, SHA))
			RunEx("Cannot add branch '") << branch << "' with SHA '" << SHA << '\'' <<
							raise;
	}

	if (!opts.sqliteCreateOnly) {
		Clr(Clr::GREEN) << "== " << branchNote << " -- Retrieving supported info ==";
		auto supp = getSupported(commit);

		Clr(Clr::GREEN) << "== " << branchNote << " -- Running file2config ==";
		auto visitor = getMakeVisitor(sql, supp, branch, root);
		TW::TreeWalker tw(root, *visitor);
		tw.walk();

		if (sql) {
			Clr(Clr::GREEN) << "== " << branchNote << " -- Collecting configs ==";
			processConfigs(sql, branch, repo, commit);

			Clr(Clr::GREEN) << "== " << branchNote <<
					       " -- Detecting authors of patches ==";
			processAuthors(opts, sql, branch, repo, commit);

			if (ignoredFiles) {
				Clr(Clr::GREEN) << "== " << branchNote <<
						       " -- Collecting ignored files ==";
				processIgnores(sql, branch, *ignoredFiles, root);
			}
		}
	}

	if (sql) {
		Clr(Clr::GREEN) << "== " << branchNote << " -- Committing ==";
		sql->end();
	}
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

void processRenamesBetween(std::optional<SQL::F2CSQLConn> &sql, const SlGit::Repo &lrepo,
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
		RunEx("Cannot open stdout of git").raise();

	SlHelpers::PtrStore<char, decltype([](char *ptr) { free(ptr); })> lineRaw;
	size_t len = 0;

	while (getline(lineRaw.ptr(), &len, stream.get()) != -1) {
		auto line = lineRaw.str();
		if (line.empty() || line.front() != ':')
			RunEx("Bad line: ") << line << raise;

		auto vec = SlHelpers::String::splitSV(line, " \t\n");
		if (vec.size() < 7)
			RunEx("Bad formatted line: ") << line << raise;

		unsigned int similarity{};
		std::from_chars(vec[4].data() + 1, vec[4].data() + vec[4].size(), similarity);
		if (!similarity)
			RunEx("Bad rename part: ") << std::string(vec[4]) << raise;
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
		RunEx("Not completely read: ") << strerror(errno) << raise;

	if (!p.waitForFinished())
		RunEx("Cannot wait for git: ") << p.lastError() << raise;

	if (p.signalled())
		RunEx("git crashed").raise();
	if (auto e = p.exitStatus())
		RunEx("git exited with ") << e << raise;

	auto trans = sql->beginAuto();
	for (const auto &e: renames) {
		auto oldP = sql->insertPath(e.first);
		if (!oldP)
			RunEx("Cannot insert old path: ") << e.first << ": " <<
						 sql->lastError() << raise;
		auto newP = sql->insertPath(e.second.path);
		if (!newP)
			RunEx("Cannot insert new path: ") << e.second.path << ": " <<
						 sql->lastError() << raise;
		if (!sql->insertRFVMap(begVersion, e.second.similarity, oldP->first, oldP->second,
				       newP->first, newP->second))
			RunEx("Cannot insert rename file map: ") << e.first << " -> " <<
								    e.second.path << ": " <<
								    sql->lastError() << raise;
	}
}

void processRenames(std::optional<SQL::F2CSQLConn> &sql, const SlGit::Repo &lrepo,
		    const SlGit::Repo &repo, SlKernCVS::Branches::BranchesList branches)
{
	auto uniqTags = getTagsFromKsourceTree(branches, repo);
	RenameMap map;
	if (uniqTags.size() >= 1) {
		auto curr = uniqTags.rbegin();

		processRenamesBetween(sql, lrepo, *curr, "", map);
		for (auto prev = std::next(curr); prev != uniqTags.rend(); ++curr, ++prev)
			processRenamesBetween(sql, lrepo, *prev, *curr, map);
	}
}

} // namespace

void handleEx(int argc, char **argv)
{
	const auto opts = getOpts(argc, argv);

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

	SlKernCVS::Branches::BranchesList branches { std::move(opts.branches) };
	if (branches.empty()) {
		auto branchesOpt = SlKernCVS::Branches::getBuildBranches();
		if (!branchesOpt)
			RunEx("Cannot download branches.conf").raise();
		branches = *branchesOpt;
	}

	branches.insert(branches.end(), opts.appendBranches.begin(), opts.appendBranches.end());

	Clr(Clr::GREEN) << "== Fetching branches ==";

	auto remote = repo.remoteLookup("origin");
	if (!remote)
		RunEx("No origin").raise();
	if (!remote->fetchBranches(branches, 1, false))
		RunEx("Fetch failed: ") << repo.lastError() << " (" << repo.lastClass() << ')' <<
					   raise;

	auto sql = getSQL(opts);

	auto ignoredFiles = loadIgnoredFiles(opts);

	auto branchNo = 0U;
	auto branchCnt = branches.size();

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
			      ignoredFiles);
	}

	if (sql) {
		Clr(Clr::GREEN) << "== Collecting renames ==";
		processRenames(sql, *lrepo, repo, branches);

		if (!sql->exec("VACUUM;"))
			RunEx("Cannot VACUUM the DB: ") << sql->lastError() << raise;
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
