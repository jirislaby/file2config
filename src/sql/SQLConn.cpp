// SPDX-License-Identifier: GPL-2.0-only

#include <chrono>
#include <thread>
#include <typeindex>
#include <vector>

#include "SQLConn.h"

using namespace SQL;

static int busy_handler(void *, int count)
{
	static const auto WAIT_INTVAL = std::chrono::milliseconds(20);
	static const auto WAIT_TIMEOUT = std::chrono::seconds(10) / WAIT_INTVAL;

	if (count >= WAIT_TIMEOUT)
		return 0;

	std::this_thread::sleep_for(WAIT_INTVAL);

	return 1;
}

static void joinVec(std::ostringstream &ss, const std::vector<std::string> &vec,
		    const std::string &sep = ", ")
{
	for (auto i = vec.begin(), end = vec.end(); i != end; ++i) {
		ss << *i;
		if (i != end - 1)
			ss << sep;
	}
}

int SQLConn::openDB(const std::filesystem::path &dbFile, unsigned int flags)
{
	sqlite3 *sql;
	int openFlags = SQLITE_OPEN_READWRITE;
	int ret;

	if (flags & CREATE)
		openFlags |= SQLITE_OPEN_CREATE;

	ret = sqlite3_open_v2(dbFile.c_str(), &sql, openFlags, NULL);
	sqlHolder.reset(sql);
	if (ret != SQLITE_OK) {
		std::cerr << "db open failed: " << sqlite3_errstr(ret) << "\n";
		return -1;
	}

	if (!(flags & NO_FOREIGN_KEY)) {
		char *err;
		ret = sqlite3_exec(sqlHolder, "PRAGMA foreign_keys = ON;", NULL, NULL, &err);
		if (ret != SQLITE_OK) {
			std::cerr << "db PRAGMA failed (" << __LINE__ << "): " <<
					sqlite3_errstr(ret) << " -> " << err << "\n";
			sqlite3_free(err);
			return -1;
		}
	}

	ret = sqlite3_busy_handler(sqlHolder, busy_handler, nullptr);
	if (ret != SQLITE_OK) {
		std::cerr << "db busy_handler failed (" << __LINE__ << "): " <<
				sqlite3_errstr(ret) << "\n";
		return -1;
	}

	return 0;
}

int SQLConn::createTables(const Tables &tables)
{
	char *err;
	int ret;

	for (const auto &c: tables) {
		std::ostringstream ss;
		ss << "CREATE TABLE IF NOT EXISTS " << c.first << '(';
		joinVec(ss, c.second);
		ss << ") STRICT;";
		ret = sqlite3_exec(sqlHolder, ss.str().c_str(), NULL, NULL, &err);
		if (ret != SQLITE_OK) {
			std::cerr << "db CREATE failed (" << __LINE__ << "): " <<
					sqlite3_errstr(ret) << " -> " <<
					err << "\n\t" << ss.str() << "\n";
			sqlite3_free(err);
			return -1;
		}
	}

	return 0;
}

int SQLConn::createIndices(const Indices &indices)
{
	char *err;
	int ret;

	for (const auto &c: indices) {
		std::string s("CREATE INDEX IF NOT EXISTS ");
		s.append(c.first).append(" ON ").append(c.second);
		ret = sqlite3_exec(sqlHolder, s.c_str(), NULL, NULL, &err);
		if (ret != SQLITE_OK) {
			std::cerr << "db CREATE failed (" << __LINE__ << "): " <<
					sqlite3_errstr(ret) << " -> " <<
					err << "\n\t" << s << "\n";
			sqlite3_free(err);
			return -1;
		}
	}

	return 0;
}

int SQLConn::createViews(const Views &views)
{
	char *err;
	int ret;

	for (const auto &c: views) {
		std::string s("CREATE VIEW IF NOT EXISTS ");
		s.append(c.first).append(" AS ").append(c.second);
		ret = sqlite3_exec(sqlHolder, s.c_str(), NULL, NULL, &err);
		if (ret != SQLITE_OK) {
			std::cerr << "db CREATE failed (" << __LINE__ << "): " <<
					sqlite3_errstr(ret) << " -> " <<
					err << "\n\t" << s << "\n";
			sqlite3_free(err);
			return -1;
		}
	}

	return 0;
}

int SQLConn::prepareStatement(const std::string &sql, SQLStmtHolder &stmt)
{
	sqlite3_stmt *SQLStmt;
	int ret = sqlite3_prepare_v2(sqlHolder, sql.c_str(), -1, &SQLStmt, NULL);
	if (ret != SQLITE_OK) {
		std::cerr << "db prepare failed (" << __LINE__ << "): " <<
			     sqlite3_errstr(ret) << " -> " <<
			     sqlite3_errmsg(sqlHolder) << "\n";
		return -1;
	}

	stmt.reset(SQLStmt);

	return 0;
}

int SQLConn::begin()
{
	char *err;
	int ret = sqlite3_exec(sqlHolder, "BEGIN;", NULL, NULL, &err);
	if (ret != SQLITE_OK) {
		std::cerr << "db BEGIN failed (" << __LINE__ << "): " <<
			     sqlite3_errstr(ret) << " -> " << err << "\n";
		sqlite3_free(err);
		return -1;
	}

	return 0;
}

int SQLConn::end()
{
	char *err;
	int ret = sqlite3_exec(sqlHolder, "END;", NULL, NULL, &err);
	if (ret != SQLITE_OK) {
		std::cerr << "db END failed (" << __LINE__ << "): " <<
			     sqlite3_errstr(ret) << " -> " << err << "\n";
		sqlite3_free(err);
		return -1;
	}

	return 0;
}

int SQLConn::bind(SQLStmtHolder &ins, const std::string &key, const std::string &val)
{
	auto bindIdx = sqlite3_bind_parameter_index(ins, key.c_str());
	if (!bindIdx) {
		std::cerr << "no index found for key=" << key << "\n";
		return -1;
	}

	int ret = sqlite3_bind_text(ins, bindIdx, val.data(), val.length(), SQLITE_STATIC);
	if (ret != SQLITE_OK) {
		std::cerr << "db bind failed (" << __LINE__ << " key=\"" << key <<
			     "\" val=\"" << val << "\"): " <<
			     sqlite3_errstr(ret) << " -> " <<
			     sqlite3_errmsg(sqlHolder) << "\n";
		return -1;
	}

	return 0;
}

int SQLConn::bind(SQLStmtHolder &ins, const Binding &binding)
{
	for (const auto &b : binding)
		if (bind(ins, b.first, b.second))
			return -1;

	return 0;
}

int SQLConn::insert(SQLStmtHolder &ins, const Binding &binding)
{
	SQLStmtResetter insSrcResetter(sqlHolder, ins);
	int ret;

	if (bind(ins, binding))
		return -1;

	ret = sqlite3_step(ins);
	if (ret != SQLITE_DONE && sqlite3_extended_errcode(sqlHolder) != SQLITE_CONSTRAINT_UNIQUE) {
		std::cerr << "db step (INSERT) failed (" << __LINE__ << "): " <<
			     sqlite3_errstr(ret) << " -> " <<
			     sqlite3_errmsg(sqlHolder) << "\n";
		return -1;
	}

	return 0;
}

int SQLConn::select(SQLStmtHolder &sel, const Binding &binding, const ColumnTypes &columns,
		    SelectResult &result)
{
	int ret;

	if (bind(sel, binding))
		return -1;

	while (true) {
		ret = sqlite3_step(sel);
		if (ret == SQLITE_DONE)
			return 0;
		if (ret != SQLITE_ROW) {
			std::cerr << "db step (SELECT) failed (" << __LINE__ << "): " <<
				     sqlite3_errstr(ret) << " -> " <<
				     sqlite3_errmsg(sqlHolder) << "\n";
			return -1;
		}

		Row row;

		for (unsigned i = 0; i < columns.size(); ++i) {
			Column col;
			if (columns[i] == typeid(int))
				col = sqlite3_column_int(sel, i);
			else if (columns[i] == typeid(std::string))
				col = Column{(char *)sqlite3_column_text(sel, i)};
			row.push_back(std::move(col));
		}
		result.push_back(std::move(row));
	}

	return 0;
}
