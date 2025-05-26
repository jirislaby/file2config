// SPDX-License-Identifier: GPL-2.0-only

#ifndef SQLCONN_H
#define SQLCONN_H

#include <filesystem>
#include <string>
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
	int openDB(const std::filesystem::path &dbFile, unsigned int flags = 0);
	virtual int createDB() { return 0; }
	virtual int prepDB() { return 0; }

	int begin();
	int end();

protected:
	SQLConn() {}

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
};

}

#endif // SQLCONN_H
