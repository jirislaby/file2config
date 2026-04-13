// SPDX-License-Identifier: GPL-2.0-only

#include <cstdlib>
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
#include <string>
#include <variant>

#include "OutputFormatter.h"

using namespace F2C;

using Clr = SlHelpers::Color;
using RunEx = SlHelpers::RuntimeException;
using SlHelpers::raise;

namespace {

class F2CSQLConn : public SlSqlite::SQLConn {
public:
	F2CSQLConn() {}

	virtual bool prepDB() override {
		static const std::string branchCTE {
			"branch_cte AS (SELECT id, branch, version FROM branch "
				"WHERE :branch = '' OR branch = :branch)"
		};
		static const std::string fileRenamedCTE {
			"file_renamed_cte AS (SELECT branch_cte.id AS branch_id, "
				"COALESCE(map.oldfile, file.id) AS file_id "
				"FROM branch_cte "
				"CROSS JOIN file "
				"JOIN dir ON file.dir = dir.id "
				"LEFT JOIN rename_file_version_map AS map ON "
					"file.id = map.newfile AND "
					"branch_cte.version = map.version "
				"WHERE dir.dir = :dir AND file.file = :file)"
		};
		return prepareStatements({
			{ selBranch, "SELECT 1 FROM branch WHERE branch = :branch;" },
			{ selConfig,
				"WITH " + branchCTE + ", " + fileRenamedCTE + ", " +
				"module_cte AS (SELECT branch_cte.id AS branch, "
					"dir.dir AS module_dir, "
					"module.module, mfmap.file AS file_id "
					"FROM branch_cte "
					"JOIN file_renamed_cte ON "
						"branch_cte.id = file_renamed_cte.branch_id "
					"JOIN module_file_map AS mfmap ON "
						"branch_cte.id = mfmap.branch AND "
						"mfmap.file = file_renamed_cte.file_id "
					"LEFT JOIN module ON mfmap.module = module.id "
					"LEFT JOIN dir ON module.dir = dir.id) "
				"SELECT branch_cte.branch, config.config, module_cte.module_dir, "
					"module_cte.module, dir.dir, file.file "
					"FROM branch_cte "
					"JOIN file_renamed_cte ON "
						"branch_cte.id = file_renamed_cte.branch_id "
					"LEFT JOIN file ON file_renamed_cte.file_id = file.id "
					"LEFT JOIN dir ON file.dir = dir.id "
					"JOIN conf_file_map AS cfmap ON "
						"branch_cte.id = cfmap.branch AND "
						"cfmap.file = file_renamed_cte.file_id "
					"LEFT JOIN module_cte ON "
						"branch_cte.id = module_cte.branch AND "
						"cfmap.file = module_cte.file_id "
					"LEFT JOIN config ON cfmap.config = config.id "
					"ORDER BY branch_cte.version, branch_cte.branch;" },
			{ selRename,
				"WITH " + branchCTE + ", " +
				"file_cte AS (SELECT file.id "
					"FROM file "
					"JOIN dir ON file.dir = dir.id "
					"WHERE dir.dir = :dir AND file.file = :file) "
				"SELECT branch_cte.branch, map.similarity, olddir.dir, "
					"oldfile.file "
					"FROM branch_cte "
					"JOIN rename_file_version_map AS map ON "
						"branch_cte.version = map.version "
					"LEFT JOIN file AS oldfile ON map.oldfile = oldfile.id "
					"LEFT JOIN dir AS olddir ON oldfile.dir = olddir.id "
					"WHERE map.newfile = (SELECT id FROM file_cte)"
					"ORDER BY branch_cte.version, branch_cte.branch;" },
			{ selModuleDetails,
				"WITH " + branchCTE + " " +
				"SELECT branch_cte.id, module.id, branch_cte.branch, mdir.dir, "
					"mdmap.supported, config.config "
					"FROM branch_cte "
					"JOIN module_details_map AS mdmap ON "
						"branch_cte.id = mdmap.branch "
					"INNER JOIN module ON mdmap.module = module.id "
					"LEFT JOIN dir AS mdir ON module.dir = mdir.id "
					"LEFT JOIN config ON module.config = config.id "
					"WHERE module.module = :module "
					"ORDER BY branch_cte.version, branch_cte.branch;" },
			{ selModuleFiles,
				"SELECT dir.dir, file.file "
					"FROM module_file_map AS mfmap "
					"LEFT JOIN file ON mfmap.file = file.id "
					"LEFT JOIN dir ON file.dir = dir.id "
					"WHERE mfmap.branch = :branch_id AND "
						"mfmap.module = :module_id "
					"ORDER BY dir.dir, file.file;" },
			});
	}

	auto selectBranch(const std::string &branch) const noexcept {
		return select(selBranch, { { ":branch", branch } });
	}

	auto selectConfig(const std::string &branch, const std::string &dir,
			  const std::string &file) const {
		return select(selConfig, {
			      { ":branch", branch },
			      { ":dir", dir },
			      { ":file", file },
			      });
	}

	auto selectRename(const std::string &branch, const std::string &dir,
			  const std::string &file) const {
		return select(selRename, {
			      { ":branch", branch },
			      { ":dir", dir },
			      { ":file", file },
			      });
	}

	auto selectModuleDetails(const std::string &branch, const std::string &module) const {
		return select(selModuleDetails, {
			      { ":branch", branch },
			      { ":module", module },
			      });
	}

	auto selectModuleFiles(const int branchID, const int moduleID) const {
		return select(selModuleFiles, {
			      { ":branch_id", branchID },
			      { ":module_id", moduleID },
			      });
	}
private:
	SlSqlite::SQLStmtHolder selBranch;
	SlSqlite::SQLStmtHolder selConfig;
	SlSqlite::SQLStmtHolder selModule;
	SlSqlite::SQLStmtHolder selRename;
	SlSqlite::SQLStmtHolder selModuleDetails;
	SlSqlite::SQLStmtHolder selModuleFiles;
};

struct Opts {
	bool refresh;

	std::filesystem::path kernelTree;
	std::filesystem::path sqlite;
	bool hasSqlite;

	bool configs;
	bool renames;
	bool modules;

	std::string branch;
	std::vector<std::filesystem::path> files;
	std::vector<std::string> shas;

	bool module;

	bool json;

	std::unique_ptr<OutputFormatter> formatter;
};

Opts getOpts(int argc, char **argv)
{
	cxxopts::Options options { argv[0], "Client for the conf_file_map database" };
	Opts opts {};
	options.add_options()
		("h,help", "Print this help message")
		("r,refresh", "Refresh the db file",
			cxxopts::value(opts.refresh)->default_value("false"))
	;
	options.add_options("Paths")
		("k,kernel-tree", "Clone of the mainline kernel repo",
			cxxopts::value(opts.kernelTree)->default_value("$LINUX_GIT"))
		("sqlite", "Path to the db",
			cxxopts::value(opts.sqlite)->default_value("S-G-M_cache_dir/conf_file_map.sqlite"))
	;
	options.add_options("General query")
		("b,branch", "Branch to query", cxxopts::value(opts.branch))
		("f,file", "files to use for searching; - for stdin. "
			  "This option can be provided multiple times with different values.",
			cxxopts::value(opts.files))
		("s,sha", "SHA of a commit from which to extract filenames; - for stdin. "
			  "This option can be provided multiple times with different values. "
			  "SHA could be in any form accepted by git-rev-parse.",
			cxxopts::value(opts.shas))
	;
	options.add_options("Action")
		("configs", "Find configs (and modules) of the specified files (default if no action specified)",
			cxxopts::value(opts.configs)->default_value("false"))
		("modules", "Find modules specified by files",
			cxxopts::value(opts.modules)->default_value("false"))
		("renames", "Find renames of the specified files",
			cxxopts::value(opts.renames)->default_value("false"))
	;
	options.add_options("Configs query")
		("m,module", "Include also module path in the output", cxxopts::value(opts.module))
	;
	options.add_options("Output")
		("force-color", "Force color output")
		("j,json", "Output JSON",
			cxxopts::value(opts.json)->default_value("false"))
	;

	try {
		// General
		auto cxxopts = options.parse(argc, argv);
		if (cxxopts.contains("help")) {
			std::cout << options.help();
			exit(0);
		}
		Clr::forceColor(cxxopts.contains("force-color"));

		// Paths
		if (!cxxopts.contains("kernel-tree"))
			if (const auto path = SlHelpers::Env::get<std::filesystem::path>("LINUX_GIT"))
				opts.kernelTree = *path;
		opts.hasSqlite = cxxopts.contains("sqlite");

		// Action
		if (cxxopts.contains("configs") + cxxopts.contains("modules") +
		    cxxopts.contains("renames") == 0)
			opts.configs = true;

		return opts;
	} catch (const cxxopts::exceptions::parsing &e) {
		Clr(std::cerr, Clr::RED) << "arguments error: " << e.what();
		std::cerr << options.help();
		exit(EXIT_FAILURE);
	}
}

void checkBranch(const F2CSQLConn &sql, const std::string &branch)
{
	if (branch.empty())
		return;
	const auto sel = sql.selectBranch(branch);
	if (!sel || !sel->size() || std::get<int>(sel->at(0)[0]) != 1 )
		RunEx("Branch '") << branch << "' not found in the DB!" << raise;
}

template<typename T, typename Callback>
requires std::invocable<Callback, T> &&
	std::invocable<Callback, std::string> && // for stdin
	std::equality_comparable_with<T, const char *>
void handleCmdlineFile(T &&file, Callback &&callback)
{
	if (file != "-") {
		std::invoke(callback, std::forward<T>(file));
		return;
	}

	for (std::string line; std::getline(std::cin, line);)
		std::invoke(callback, SlHelpers::String::trim(line));
}

template<std::ranges::input_range FileTy, typename Callback>
void handleCmdlineFiles(FileTy &&files, Callback &&callback)
{
	for (auto &&f: files)
		handleCmdlineFile(std::forward<decltype(f)>(f), callback);
}

void setFormatter(Opts &opts)
{
	if (opts.json)
		opts.formatter = std::make_unique<OutputFormatterJSON>();
	else
		opts.formatter = std::make_unique<OutputFormatterSimple>(opts.module,
									 opts.branch.empty());
}

void selectRenamesQuery(const Opts &opts, const F2CSQLConn &sql, const std::filesystem::path &dir,
			const std::filesystem::path &file) noexcept
{
	auto res = sql.selectRename(opts.branch, dir, file);
	if (!res)
		return;

	for (auto &row: *res) {
		auto branch = std::get<std::string>(std::move(row[0]));
		std::filesystem::path oldDir(std::get<std::string>(std::move(row[2])));
		std::filesystem::path oldFile(std::get<std::string>(std::move(row[3])));

		opts.formatter->addRename(oldDir / oldFile, dir / file, branch,
					  std::get<int>(row[1]));
	}
}

void selectConfigQuery(const Opts &opts, const F2CSQLConn &sql, const std::filesystem::path &dir,
		       const std::filesystem::path &file) noexcept
{
	auto res = sql.selectConfig(opts.branch, dir, file);
	if (!res)
		return;

	for (auto &conf: *res) {
		auto branch = std::get<std::string>(std::move(conf[0]));
		auto config = std::get<std::string>(std::move(conf[1]));
		std::filesystem::path mod;
		if (opts.module || opts.json) {
			mod = std::get<std::string>(std::move(conf[2]));
			mod /= std::get<std::string>(std::move(conf[3]));
		}
		std::filesystem::path oldFile = std::get<std::string>(std::move(conf[4]));
		oldFile /= std::get<std::string>(std::move(conf[5]));
		opts.formatter->addConfig(oldFile, branch, config, mod);
	}
}

void selectRenameConfigQuery(const Opts &opts, const F2CSQLConn &sql,
		 const std::filesystem::path &path) noexcept
{
	auto dir = path.parent_path();
	auto file = path.filename();
	if (opts.renames)
		selectRenamesQuery(opts, sql, dir, file);
	if (opts.configs)
		selectConfigQuery(opts, sql, dir, file);
}

void selectModuleQuery(const Opts &opts, const F2CSQLConn &sql,
		       const std::filesystem::path &module) noexcept
{
	auto res = sql.selectModuleDetails(opts.branch, module);
	if (!res)
		return;

	for (auto &row: *res) {
		auto branchID = std::get<int>(row[0]);
		auto moduleID = std::get<int>(row[1]);
		auto branch = std::get<std::string>(std::move(row[2]));
		std::filesystem::path modDir = std::get<std::string>(std::move(row[3]));
		auto supported = std::get<int>(row[4]);
		std::string config { "n/a" };
		if (std::holds_alternative<std::string>(row[5]))
			config = std::get<std::string>(std::move(row[5]));
		opts.formatter->addModule(modDir / module, branch, supported, config);

		auto res2 = sql.selectModuleFiles(branchID, moduleID);
		if (!res2)
			continue;

		for (auto &row2: *res2) {
			std::filesystem::path file { std::get<std::string>(std::move(row2[0])) };
			file /= std::get<std::string>(std::move(row2[1]));
			opts.formatter->addModuleFile(file);
		}
	}
}

void handleOne(const Opts &opts, const F2CSQLConn &sql, const std::filesystem::path &path) noexcept
{
	if (opts.configs || opts.renames)
		selectRenameConfigQuery(opts, sql, path);
	else if (opts.modules)
		selectModuleQuery(opts, sql, path);
}

void handleFiles(const Opts &opts, const F2CSQLConn &sql) noexcept
{
	handleCmdlineFiles(opts.files, [&sql, &opts](const std::filesystem::path &file) {
		opts.formatter->newObj("file", file.string());
		handleOne(opts, sql, file);
	});
}

void handleSHA(const Opts &opts, const F2CSQLConn &sql, const SlGit::Repo &repo,
	       const std::string &sha)
{
	const auto commit = repo.commitRevparseSingle(sha);
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
			handleOne(opts, sql, f);
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

	handleCmdlineFiles(opts.shas, [&sql, &opts, &rk](const std::string &sha) {
		opts.formatter->newObj("sha", sha);
		handleSHA(opts, sql, rk, sha);
	});
}

void handleEx(int argc, char **argv)
{
	auto opts = getOpts(argc, argv);

	setFormatter(opts);

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

	checkBranch(sql, opts.branch);

	handleFiles(opts, sql);
	handleSHAs(opts, sql);

	opts.formatter->print();
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
