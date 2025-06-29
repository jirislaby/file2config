// SPDX-License-Identifier: GPL-2.0-only

#include <getopt.h>
#include <iostream>

#include "sql/F2CSQLConn.h"
#include "treewalker/ConsoleMakeVisitor.h"
#include "treewalker/SQLiteMakeVisitor.h"
#include "treewalker/TreeWalker.h"
#include "Verbose.h"

static void usage(const char *prgname)
{
	std::cout << prgname << ": [-O] [-q] [-s [dbFile]] [-S] [-v] [start_path]\n";
}

std::unique_ptr<SQL::F2CSQLConn> getSQL(bool sqlite, const std::filesystem::path &DBPath,
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

std::unique_ptr<TW::MakeVisitor> getMakeVisitor(const std::unique_ptr<SQL::F2CSQLConn> &sql,
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
	const struct option opts[] = {
		{ "quiet", 0, nullptr, 'q' },
		{ "sqlite", 2, nullptr, 's' },
		{ "sqlite-branch", 1, nullptr, 1000 },
		{ "sqlite-SHA", 1, nullptr, 1001 },
		{ "sqlite-create", 0, nullptr, 'S' },
		{ "sqlite-create-only", 0, nullptr, 'O' },
		{ "verbose", 0, nullptr, 'v' },
		{}
	};

	bool sqlite = false;
	bool sqliteCreate = false;
	bool skipWalk = false;
	std::string sqliteBranch { "unknown-branch" };
	std::string sqliteSHA { "unknown" };
	std::filesystem::path sqliteDB { "conf_file_map.sqlite" };

	int opt;
	while ((opt = getopt_long(argc, argv, "Oqs::Sv", opts, nullptr)) >= 0) {
		switch (opt) {
		case 'O':
			sqliteCreate = true;
			skipWalk = true;
			break;
		case 'q':
			F2C::quiet = true;
			break;
		case 's':
			sqlite = true;
			if (optarg)
				sqliteDB = optarg;
			break;
		case 'S':
			sqliteCreate = true;
			break;
		case 'v':
			F2C::verbose++;
			break;
		case 1000:
			sqliteBranch = optarg;
			break;
		case 1001:
			sqliteSHA = optarg;
			break;
		default:
			usage(argv[0]);
			return 0;
		}

	}

	std::filesystem::path root{"."};
	if (argc > optind)
		root = argv[optind];

	auto sql = getSQL(sqlite, sqliteDB, sqliteCreate, skipWalk);
	if (sqlite && !sql)
		return EXIT_FAILURE;

	if (processBranch(sql, sqliteBranch, sqliteSHA, skipWalk, root))
		return EXIT_FAILURE;

	return 0;
}
