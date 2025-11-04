// SPDX-License-Identifier: GPL-2.0-only

#include <cxxopts.hpp>
#include <chrono>
#include <iostream>

#include <sl/curl/Curl.h>
#include <sl/git/Git.h>
#include <sl/helpers/Color.h>
#include <sl/helpers/Misc.h>
#include <sl/helpers/Process.h>
#include <sl/helpers/PushD.h>
#include <sl/helpers/String.h>
#include <sl/sqlite/SQLConn.h>

using Clr = SlHelpers::Color;

namespace {

class F2CSQLConn : public SlSqlite::SQLConn {
public:
	F2CSQLConn() {}

	virtual bool prepDB() override {
		return prepareStatement("SELECT config.config "
					"FROM conf_file_map AS cfmap "
					"LEFT JOIN config ON cfmap.config = config.id "
					"WHERE branch = (SELECT id "
						"FROM branch "
						"WHERE branch = :branch) AND "
					"cfmap.file = (SELECT file.id "
						"FROM file "
						"LEFT JOIN dir ON file.dir = dir.id "
						"WHERE dir.dir = :dir AND file.file = :file);",
					selConfig);
	}

	auto selectConfig(const std::string &branch, const std::string &dir,
			  const std::string &file) const {
		return select(selConfig, {
			      { ":branch", branch },
			      { ":dir", dir },
			      { ":file", file },
			      }, { typeid(std::string) });
	}
private:
	SlSqlite::SQLStmtHolder selConfig;
};

struct Opts {
	bool refresh;
	std::filesystem::path kernelTree;
	std::filesystem::path sqlite;
	bool hasSqlite;
	std::string branch;
	std::vector<std::filesystem::path> files;
	std::vector<std::string> shas;
};

Opts getOpts(int argc, char **argv)
{
	cxxopts::Options options { argv[0], "Client for the conf_file_map database" };
	Opts opts;
	options.add_options()
		("h,help", "Print this help message")
		("force-color", "Force color output")
		("r,refresh", "Refresh the db file",
			cxxopts::value(opts.refresh)->default_value("false"))
	;
	options.add_options("paths")
		("k,kernel-tree", "Clone of the mainline kernel repo",
			cxxopts::value(opts.kernelTree)->default_value("$LINUX_GIT"))
		("sqlite", "Path to the db",
			cxxopts::value(opts.sqlite)->default_value("S-G-M_cache_dir/conf_file_map.sqlite"))
	;
	options.add_options("query")
		("b,branch", "branch to query", cxxopts::value(opts.branch))
		("f,file", "file for which to find configs of; - for stdin. "
			  "This option can be provided multiple times with different values.",
			cxxopts::value(opts.files))
		("s,sha", "SHA of a commit for which to find configs of; - for stdin. "
			  "This option can be provided multiple times with different values. "
			  "SHA could be in any form accepted by git-rev-parse.",
			cxxopts::value(opts.shas))
	;

	try {
		auto cxxopts = options.parse(argc, argv);
		if (cxxopts.contains("help")) {
			std::cout << options.help();
			exit(0);
		}
		Clr::forceColor(cxxopts.contains("force-color"));
		opts.hasSqlite = cxxopts.contains("sqlite");
		if (!cxxopts.contains("branch")) {
			Clr(std::cerr, Clr::RED) << "branch not specified";
			std::cerr << options.help();
			exit(EXIT_FAILURE);
		}
		if (!cxxopts.contains("kernel-tree"))
			if (const auto path = SlHelpers::Env::get<std::filesystem::path>("LINUX_GIT"))
				opts.kernelTree = *path;
		return opts;
	} catch (const cxxopts::exceptions::parsing &e) {
		Clr(std::cerr, Clr::RED) << "arguments error: " << e.what();
		std::cerr << options.help();
		exit(EXIT_FAILURE);
	}
}

template<typename T = std::string_view>
bool handleCmdlineFile(const std::string &file,
		       const std::function<bool (const T &)> &callback)
{
	if (file != "-")
		return callback(file);

	for (std::string line; std::getline(std::cin, line);)
		if (!callback(SlHelpers::String::trim(std::string_view(line))))
			return false;

	return true;
}

template<typename ParamTy = std::string_view, typename FileTy>
bool handleCmdlineFiles(const FileTy &files,
			const std::function<bool (const ParamTy &)> &callback)
{
	for (const auto &f: files)
		if (!handleCmdlineFile<ParamTy>(f, callback))
			return false;

	return true;
}

bool selectConfigQuery(const F2CSQLConn &sql, const std::string &branch,
		       const std::filesystem::path &file)
{
	auto res = sql.selectConfig(branch, file.parent_path(), file.filename());
	if (!res || res->size() == 0)
		return true;
	for (const auto &conf: *res)
		std::cout << file.string() << " " << std::get<std::string>(conf[0]) << '\n';
	return true;
}

bool handleFiles(const Opts &opts, const F2CSQLConn &sql)
{
	return handleCmdlineFiles<std::filesystem::path>(opts.files,
			[&sql, &branch = opts.branch](const auto &file) {
		return selectConfigQuery(sql, branch, file);
	});
}

bool handleSHA(const F2CSQLConn &sql, const std::string &branch, const SlGit::Repo &repo,
		const std::string_view &sha)
{
	const auto commit = repo.commitRevparseSingle(std::string(sha));
	if (!commit)
		return false;

	if (commit->parentCount() > 1) {
		Clr(std::cerr, Clr::YELLOW) << sha << " is a merge commit, skipping";
		return true;
	}

	const auto diff = repo.diff(*commit, *commit->parent());
	if (!diff)
		return false;

	SlGit::Diff::ForEachCB cb = {
		.file = [&sql, &branch](const git_diff_delta &delta, float) {
			std::filesystem::path f(delta.new_file.path);
			return selectConfigQuery(sql, branch, f) ? 0 : -1;
		},
	};

	if (diff->forEach(cb))
		return false;

	return true;
}

bool handleSHAs(const Opts &opts, const F2CSQLConn &sql)
{
	if (opts.shas.empty())
		return true;

	auto rkOpt = SlGit::Repo::open(opts.kernelTree);
	if (!rkOpt) {
		Clr(std::cerr, Clr::RED) << "Unable to open kernel tree: " <<
					    git_error_last()->message;
		return false;
	}
	const auto rk = std::move(*rkOpt);

	return handleCmdlineFiles<std::string_view>(opts.shas,
			[&sql, &branch = opts.branch, &rk](const auto &sha) {
		return handleSHA(sql, branch, rk, sha);
	});
}

} // namespace

int main(int argc, char **argv)
{
	auto opts = getOpts(argc, argv);

	const auto SGMCacheDir = SlHelpers::HomeDir::createCacheDir("suse-get-maintainers");
	if (SGMCacheDir.empty()) {
		std::cerr << "Unable to create a cache dir\n";
		return EXIT_FAILURE;
	}

	if (!opts.hasSqlite) {
		opts.sqlite = SlCurl::LibCurl::fetchFileIfNeeded(SGMCacheDir / "conf_file_map.sqlite",
								 "https://kerncvs.suse.de/conf_file_map.sqlite",
								 opts.refresh, false,
								 std::chrono::days{7});
	}

	F2CSQLConn sql;
	if (!sql.open(opts.sqlite)) {
		Clr(std::cerr, Clr::RED) << "Unable to open the db " << opts.sqlite << ": " <<
					    sql.lastError();
		return EXIT_FAILURE;
	}

	if (!handleFiles(opts, sql))
		return EXIT_FAILURE;

	if (!handleSHAs(opts, sql))
		return EXIT_FAILURE;


	return 0;
}
