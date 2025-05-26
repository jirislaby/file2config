// SPDX-License-Identifier: GPL-2.0-only

#include <getopt.h>
#include <iostream>

#include "sql/SQLConn.h"
#include "treewalker/ConsoleMakeVisitor.h"
#include "treewalker/SQLiteMakeVisitor.h"
#include "treewalker/TreeWalker.h"
#include "Verbose.h"

static void usage(const char *prgname)
{
	std::cout << prgname << ": [-O] [-q] [-s [dbFile]] [-S] [-v] [start_path]\n";
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

	std::filesystem::path path{"."};
	if (argc > optind)
		path = argv[optind];

	std::unique_ptr<TW::MakeVisitor> visitor;
	std::unique_ptr<SQL::SQLConn> sql;
	if (sqlite) {
		sql = std::make_unique<SQL::SQLConn>();
		unsigned openFlags = 0;
		if (sqliteCreate)
			openFlags |= SQL::CREATE;
		int ret = sql->openDB(sqliteDB, openFlags);
		if (ret)
			return EXIT_FAILURE;
		if (sqliteCreate) {
			ret = sql->createDB();
			if (ret)
				return EXIT_FAILURE;
		}
		if (!skipWalk) {
			ret = sql->prepDB();
			if (ret)
				return EXIT_FAILURE;
			sql->begin();
		}
		if (sql->insertBranch(sqliteBranch, sqliteSHA)) {
			std::cerr << "cannot add branch '" << sqliteBranch <<
				     "' with SHA '" << sqliteSHA << "'\n";
			return EXIT_FAILURE;
		}
		visitor = std::make_unique<TW::SQLiteMakeVisitor>(*sql, sqliteBranch, path);
	} else
		visitor = std::make_unique<TW::ConsoleMakeVisitor>();

	if (!skipWalk) {
		TW::TreeWalker tw(path, *visitor);
		tw.walk();
		if (sqlite)
			sql->end();
	}

	return 0;
}
