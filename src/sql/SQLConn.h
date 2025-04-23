// SPDX-License-Identifier: GPL-2.0-only

#ifndef SQLCONN_H
#define SQLCONN_H

#include <filesystem>
#include <typeindex>
#include <variant>
#include <vector>

#include "SQLiteSmart.h"

namespace SQL {

class SQLConn {
public:
	using Column = std::variant<int, std::string>;
	using Row = std::vector<Column>;

	SQLConn() {}

	int openDB(const std::filesystem::path &dbFile);
	int createDB();
	int prepDB();

	int begin();
	int end();

	int insertBranch(const std::string &branch, const std::string &sha);
	int insertConfig(const std::string &config);
	int insertDir(const std::string &dir);
	int insertFile(const std::string &dir, const std::string &file);
	int insertCFMap(const std::string &branch, const std::string &config, const std::string &dir, const std::string &file);
private:
	int bind(SQLStmtHolder &ins, const std::string &key, const std::string &val);
	int bind(SQLStmtHolder &ins, const std::vector<std::pair<std::string, std::string> > &binding);
	int insert(SQLStmtHolder &ins, const std::vector<std::pair<std::string, std::string> > &binding);
	int select(SQLStmtHolder &sel, const std::vector<std::pair<std::string, std::string> > &binding,
		   const std::vector<std::type_index> &columns, std::vector<Row> &result);

	SQLHolder sqlHolder;
	SQLStmtHolder insBranch;
	SQLStmtHolder insConfig;
	SQLStmtHolder insDir;
	SQLStmtHolder insFile;
	SQLStmtHolder insCFMap;
};

}

#endif // SQLCONN_H
