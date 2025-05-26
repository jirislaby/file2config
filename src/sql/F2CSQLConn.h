// SPDX-License-Identifier: GPL-2.0-only

#ifndef F2CSQLCONN_H
#define F2CSQLCONN_H

#include "SQLConn.h"
#include "SQLiteSmart.h"

namespace SQL {

class F2CSQLConn : public SQLConn {
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

#endif // F2CSQLCONN_H
