// SPDX-License-Identifier: GPL-2.0-only

#include <cxxopts.hpp>
#include <iostream>

#include "sql/F2CSQLConn.h"
#include "treewalker/ConsoleMakeVisitor.h"
#include "treewalker/SQLiteMakeVisitor.h"
#include "treewalker/TreeWalker.h"
#include "Verbose.h"

static cxxopts::ParseResult getOpts(int argc, char **argv)
{
	cxxopts::Options options { argv[0], "Generate conf_file_map database (and more)" };
	options.add_options()
		("root", "root to search in",
			cxxopts::value<std::filesystem::path>()->default_value("."))
		("q,quiet", "quiet mode")
		("v,verbose", "verbose mode")
	;
	options.add_options("sqlite")
		("s,sqlite", "create db",
			cxxopts::value<std::filesystem::path>()->
			implicit_value("conf_file_map.sqlite"))
		("sqlite-branch", "branch to use for db",
			cxxopts::value<std::string>()->default_value("unknown-branch"))
		("sqlite-SHA", "SHA of the branch",
			cxxopts::value<std::string>()->default_value("unknown"))
		("S,sqlite-create", "create the db if not exists")
		("O,sqlite-create-only", "only create the db (do not fill it)")
	;
	options.positional_help("root_to_search_in");
	options.parse_positional("root");

	try {
		return options.parse(argc, argv);
	} catch (const cxxopts::exceptions::parsing &e) {
		std::cerr << "arguments error: " << e.what() << '\n';
		std::cerr << options.help();
		exit(EXIT_FAILURE);
	}
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

static std::unique_ptr<TW::MakeVisitor> getMakeVisitor(const std::unique_ptr<SQL::F2CSQLConn> &sql,
						       const std::string &branch,
						       const std::filesystem::path &root)
{
	if (sql)
		return std::make_unique<TW::SQLiteMakeVisitor>(*sql, branch, root);
	else
		return std::make_unique<TW::ConsoleMakeVisitor>();
}

int processBranch(const std::unique_ptr<SQL::F2CSQLConn> &sql, const std::string &branch,
		  const std::string &SHA, bool skipWalk, const std::filesystem::path &root)
{
	if (sql) {
		sql->begin();
		if (sql->insertBranch(branch, SHA)) {
			std::cerr << "cannot add branch '" << branch << "' with SHA '" << SHA << "'\n";
			return -1;
		}
	}

	if (!skipWalk) {
		auto visitor = getMakeVisitor(sql, branch, root);
		TW::TreeWalker tw(root, *visitor);
		tw.walk();
	}

	if (sql)
		sql->end();

	return 0;
}

int main(int argc, char **argv)
{
	auto opts = getOpts(argc, argv);

	F2C::quiet = opts.contains("quiet");
	F2C::verbose = opts.count("verbose");

	auto sqlite = opts.contains("sqlite");
	auto sqliteDB = sqlite ? opts["sqlite"].as<std::filesystem::path>() : "";
	auto skipWalk = opts.contains("sqlite-create-only");
	auto sql = getSQL(sqlite, sqliteDB, opts.contains("sqlite-create"), skipWalk);
	if (sqlite && !sql)
		return EXIT_FAILURE;

	auto root = opts["root"].as<std::filesystem::path>();
	auto sqliteBranch = opts["sqlite-branch"].as<std::string>();
	auto sqliteSHA = opts["sqlite-SHA"].as<std::string>();
	if (processBranch(sql, sqliteBranch, sqliteSHA, skipWalk, root))
		return EXIT_FAILURE;

	return 0;
}
