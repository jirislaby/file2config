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
	virtual ~F2CSQLConn() = default;

	F2CSQLConn(F2CSQLConn &&) = default;
	F2CSQLConn &operator=(F2CSQLConn &&) = default;

	virtual bool createDB() override;
	virtual bool prepDB() override;

	bool insertBranch(const std::string &branch, const std::string &sha, unsigned version);
	bool insertConfig(const std::string &config);
	bool insertArch(const std::string &arch);
	bool insertFlavor(const std::string &flavor);
	bool insertCBMap(const std::string &branch, const std::string &arch,
			 const std::string &flavor, const std::string &config, const std::string &value);
	bool insertDir(const std::string &dir);
	bool insertFile(const std::string &dir, const std::string &file);
	std::optional<std::pair<std::string, std::string>>
		insertPath(const std::filesystem::path &path);
	bool insertCFMap(const std::string &branch, const std::string &config, const std::string &dir,
			 const std::string &file);
	bool insertConfDep(const std::string &branch, const std::string &parent,
			   const std::string &child);
	bool insertModule(const std::string &dir, const std::string &module);
	bool insertMDMap(const std::string &branch, const std::string &module_dir,
			 const std::string &module, int supported);
	bool insertMFMap(const std::string &branch, const std::string &module_dir,
			 const std::string &module, const std::string &dir, const std::string &file);
	bool insertUser(const std::string &email);
	bool insertUFMap(const std::string &branch, const std::string &email, const std::string &dir,
			 const std::string &file, int count, int countnf);
	bool insertIFBMap(const std::string &branch, const std::string &dir, const std::string &file);
	bool insertRFVMap(unsigned int version, unsigned similarity,
			  const std::string &olddir, const std::string &oldfile,
			  const std::string &newdir, const std::string &newfile);
	bool deleteBranch(const std::string &branch);
	bool hasBranch(const std::string &branch);
private:
	SlSqlite::SQLStmtHolder insBranch;
	SlSqlite::SQLStmtHolder insConfig;
	SlSqlite::SQLStmtHolder insArch;
	SlSqlite::SQLStmtHolder insFlavor;
	SlSqlite::SQLStmtHolder insCBMap;
	SlSqlite::SQLStmtHolder insDir;
	SlSqlite::SQLStmtHolder insFile;
	SlSqlite::SQLStmtHolder insCFMap;
	SlSqlite::SQLStmtHolder insConfDep;
	SlSqlite::SQLStmtHolder insModule;
	SlSqlite::SQLStmtHolder insMDMap;
	SlSqlite::SQLStmtHolder insMFMap;
	SlSqlite::SQLStmtHolder insUser;
	SlSqlite::SQLStmtHolder insUFMap;
	SlSqlite::SQLStmtHolder insIFBMap;
	SlSqlite::SQLStmtHolder insRFVMap;
	SlSqlite::SQLStmtHolder delBranch;
	SlSqlite::SQLStmtHolder selBranch;
};

}

#endif // F2CSQLCONN_H
