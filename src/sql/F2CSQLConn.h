// SPDX-License-Identifier: GPL-2.0-only

#ifndef F2CSQLCONN_H
#define F2CSQLCONN_H

#include <optional>
#include <sl/sqlite/SQLConn.h>
#include <sl/sqlite/SQLiteSmart.h>

namespace SQL {

class F2CSQLConn : public SlSqlite::SQLConn {
public:
	F2CSQLConn() {}

	virtual int createDB();
	virtual int prepDB();

	int insertBranch(const std::string &branch, const std::string &sha);
	int insertConfig(const std::string &config);
	int insertDir(const std::string &dir);
	int insertFile(const std::string &dir, const std::string &file);
	int insertCFMap(const std::string &branch, const std::string &config, const std::string &dir,
			const std::string &file);
	int insertModule(const std::string &dir, const std::string &module);
	int insertMFMap(const std::string &branch, const std::string &module_dir,
			const std::string &module, const std::string &dir, const std::string &file);
	int insertUser(const std::string &email);
	int insertUFMap(const std::string &branch, const std::string &email, const std::string &dir,
			const std::string &file, int count, int countnf);
	int deleteBranch(const std::string &branch);
	std::optional<bool> hasBranch(const std::string &branch);
private:
	SlSqlite::SQLStmtHolder insBranch;
	SlSqlite::SQLStmtHolder insConfig;
	SlSqlite::SQLStmtHolder insDir;
	SlSqlite::SQLStmtHolder insFile;
	SlSqlite::SQLStmtHolder insCFMap;
	SlSqlite::SQLStmtHolder insModule;
	SlSqlite::SQLStmtHolder insMFMap;
	SlSqlite::SQLStmtHolder insUser;
	SlSqlite::SQLStmtHolder insUFMap;
	SlSqlite::SQLStmtHolder delBranch;
	SlSqlite::SQLStmtHolder selBranch;
};

}

#endif // F2CSQLCONN_H
