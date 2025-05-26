// SPDX-License-Identifier: GPL-2.0-only

#ifndef SQLCONN_H
#define SQLCONN_H

#include <filesystem>
#include <typeindex>
#include <variant>
#include <vector>

#include "SQLiteSmart.h"

namespace SQL {

enum open_flags {
	CREATE		= 1 << 0,
	NO_FOREIGN_KEY	= 1 << 1,
};

class SQLConn {
public:
	SQLConn() {}

	int openDB(const std::filesystem::path &dbFile, unsigned int flags = 0);
	int createDB();
	int prepDB();

	int begin();
	int end();

	int insertBranch(const std::string &branch, const std::string &sha);
	int insertConfig(const std::string &config);
	int insertDir(const std::string &dir);
	int insertFile(const std::string &dir, const std::string &file);
	int insertCFMap(const std::string &branch, const std::string &config, const std::string &dir,
			const std::string &file);
	int insertModule(const std::string &dir, const std::string &module);
	int insertMFMap(const std::string &branch, const std::string &module_dir,
			const std::string &module, const std::string &dir, const std::string &file);
protected:
	using Tables = std::vector<std::pair<std::string, std::vector<std::string>>>;
	using Indices = std::vector<std::pair<std::string, std::string>>;
	using Views = Indices;

	using Binding = std::vector<std::pair<std::string, std::string>>;
	using ColumnTypes = std::vector<std::type_index>;
	using Column = std::variant<int, std::string>;
	using Row = std::vector<Column>;
	using SelectResult = std::vector<Row>;

	int createTables(const Tables &tables);
	int createIndices(const Indices &indices);
	int createViews(const Views &views);

	int prepareStatement(const std::string &sql, SQLStmtHolder &stmt);

	int bind(SQLStmtHolder &ins, const std::string &key, const std::string &val);
	int bind(SQLStmtHolder &ins, const Binding &binding);
	int insert(SQLStmtHolder &ins, const Binding &binding);
	int select(SQLStmtHolder &sel, const Binding &binding, const ColumnTypes &columns,
		   SelectResult &result);

	SQLHolder sqlHolder;

private:
	SQLStmtHolder insBranch;
	SQLStmtHolder insConfig;
	SQLStmtHolder insDir;
	SQLStmtHolder insFile;
	SQLStmtHolder insCFMap;
	SQLStmtHolder insModule;
	SQLStmtHolder insMFMap;
};

}

#endif // SQLCONN_H
