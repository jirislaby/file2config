// SPDX-License-Identifier: GPL-2.0-only

#include <cxxopts.hpp>
#include <chrono>
#include <iostream>

#include <sl/curl/Curl.h>
#include <sl/git/Git.h>
#include <sl/helpers/Color.h>
#include <sl/helpers/Exception.h>
#include <sl/helpers/HomeDir.h>
#include <sl/helpers/Misc.h>
#include <sl/helpers/String.h>
#include <sl/sqlite/SQLConn.h>

using Clr = SlHelpers::Color;
using RunEx = SlHelpers::RuntimeException;
using SlHelpers::raise;

namespace {

class F2CSQLConn : public SlSqlite::SQLConn {
public:
	F2CSQLConn() {}

	virtual bool prepDB() override {
		return prepareStatements({
			{ selConfig,
				"SELECT config.config "
					"FROM conf_file_map AS cfmap "
					"LEFT JOIN config ON cfmap.config = config.id "
					"WHERE branch = (SELECT id "
						"FROM branch "
						"WHERE branch = :branch) AND "
					"cfmap.file = (SELECT file.id "
						"FROM file "
						"LEFT JOIN dir ON file.dir = dir.id "
						"WHERE dir.dir = :dir AND file.file = :file);" },
			{ selModule,
				"SELECT module_dir.dir, module.module "
					"FROM module_file_map AS mfmap "
					"LEFT JOIN module ON mfmap.module = module.id "
					"LEFT JOIN dir AS module_dir ON "
					"	module.dir = module_dir.id "
					"WHERE mfmap.branch = (SELECT id "
					"	FROM branch "
					"	WHERE branch = :branch) AND "
					"mfmap.file IN (SELECT file.id "
					"	FROM file "
					"	LEFT JOIN dir ON file.dir = dir.id "
					"	WHERE dir.dir = :dir AND file.file = :file);",
			} });
	}

	auto selectConfig(const std::string &branch, const std::string &dir,
			  const std::string &file) const {
		return select(selConfig, {
			      { ":branch", branch },
			      { ":dir", dir },
			      { ":file", file },
			      });
	}

	auto selectModule(const std::string &branch, const std::string &dir,
			  const std::string &file) const {
		return select(selModule, {
			      { ":branch", branch },
			      { ":dir", dir },
			      { ":file", file },
			      });
	}
private:
	SlSqlite::SQLStmtHolder selConfig;
	SlSqlite::SQLStmtHolder selModule;
};

struct Opts {
	bool refresh;
	std::filesystem::path kernelTree;
	std::filesystem::path sqlite;
	bool hasSqlite;
	std::string branch;
	std::vector<std::filesystem::path> files;
	std::vector<std::string> shas;
	bool module;
};

Opts getOpts(int argc, char **argv)
{
	cxxopts::Options options { argv[0], "Client for the conf_file_map database" };
	Opts opts {};
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
		("b,branch", "Branch to query", cxxopts::value(opts.branch))
		("f,file", "file for which to find configs of; - for stdin. "
			  "This option can be provided multiple times with different values.",
			cxxopts::value(opts.files))
		("s,sha", "SHA of a commit for which to find configs of; - for stdin. "
			  "This option can be provided multiple times with different values. "
			  "SHA could be in any form accepted by git-rev-parse.",
			cxxopts::value(opts.shas))
		("m,module", "Include also module path in the output", cxxopts::value(opts.module))
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
void handleCmdlineFile(const std::string &file,
		       const std::function<void (const T &)> &callback)
{
	if (file != "-") {
		callback(file);
		return;
	}

	for (std::string line; std::getline(std::cin, line);)
		callback(SlHelpers::String::trim(std::string_view(line)));
}

template<typename ParamTy = std::string_view, typename FileTy>
void handleCmdlineFiles(const FileTy &files,
			const std::function<void (const ParamTy &)> &callback)
{
	for (const auto &f: files)
		handleCmdlineFile<ParamTy>(f, callback);
}

void selectConfigQuery(const Opts &opts, const F2CSQLConn &sql,
		       const std::filesystem::path &file) noexcept
{
	auto res = sql.selectConfig(opts.branch, file.parent_path(), file.filename());
	if (!res || res->size() == 0)
		return;

	std::string mod;
	if (opts.module) {
		auto modRes = sql.selectModule(opts.branch, file.parent_path(), file.filename());
		if (modRes && res->size() != 0)
			mod = ' ' + std::get<std::string>((*modRes)[0][0]) + '/' +
					std::get<std::string>((*modRes)[0][1]);
	}

	for (const auto &conf: *res)
		std::cout << file.string() << " " << std::get<std::string>(conf[0]) << mod << '\n';
}

void handleFiles(const Opts &opts, const F2CSQLConn &sql) noexcept
{
	handleCmdlineFiles<std::filesystem::path>(opts.files, [&sql, &opts](const auto &file) {
		selectConfigQuery(opts, sql, file);
	});
}

void handleSHA(const Opts &opts, const F2CSQLConn &sql, const SlGit::Repo &repo,
	       std::string_view sha)
{
	const auto commit = repo.commitRevparseSingle(std::string(sha));
	if (!commit)
		RunEx("Cannot find commit ") << sha << ": " << repo.lastError() << raise;

	if (commit->parentCount() > 1) {
		Clr(std::cerr, Clr::YELLOW) << sha << " is a merge commit, skipping";
		return;
	}

	const auto diff = repo.diff(*commit, *commit->parent());
	if (!diff)
		RunEx("Cannot diff commit ") << sha << " to parent: " << repo.lastError() << raise;

	SlGit::Diff::ForEachCB cb = {
		.file = [&sql, &opts](const git_diff_delta &delta, float) {
			std::filesystem::path f(delta.new_file.path);
			selectConfigQuery(opts, sql, f);
			return 0;
		},
	};

	if (diff->forEach(cb))
		RunEx("Cannot walk diff for commit ") << sha << ": " << repo.lastError() << raise;
}

void handleSHAs(const Opts &opts, const F2CSQLConn &sql)
{
	if (opts.shas.empty())
		return;

	auto rkOpt = SlGit::Repo::open(opts.kernelTree);
	if (!rkOpt)
		RunEx("Unable to open kernel tree: ") << SlGit::Repo::lastError() << raise;

	const auto rk = std::move(*rkOpt);

	handleCmdlineFiles<std::string_view>(opts.shas,
			[&sql, &opts, &rk](const auto &sha) {
		handleSHA(opts, sql, rk, sha);
	});
}

void handleEx(int argc, char **argv)
{
	auto opts = getOpts(argc, argv);

	const auto SGMCacheDir = SlHelpers::HomeDir::createCacheDir("suse-get-maintainers");
	if (SGMCacheDir.empty())
		RunEx("Unable to create a cache dir") << raise;

	if (!opts.hasSqlite) {
		opts.sqlite = SlCurl::LibCurl::fetchFileIfNeeded(SGMCacheDir / "conf_file_map.sqlite",
								 "https://kerncvs.suse.de/conf_file_map.sqlite",
								 opts.refresh, false,
								 std::chrono::days{7});
	}

	F2CSQLConn sql;
	if (!sql.open(opts.sqlite))
		RunEx("Unable to open the db ") << opts.sqlite << ": " << sql.lastError() << raise;

	handleFiles(opts, sql);
	handleSHAs(opts, sql);
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
