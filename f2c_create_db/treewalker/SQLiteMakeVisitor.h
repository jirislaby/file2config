// SPDX-License-Identifier: GPL-2.0-only

#ifndef SQLITEMAKEVISITOR_H
#define SQLITEMAKEVISITOR_H

#include "MakeVisitor.h"

namespace SlKernCVS {
class SupportedConf;
}

namespace SQL {
class F2CSQLConn;
}

namespace TW {

class SQLiteMakeVisitor : public MakeVisitor
{
public:
	SQLiteMakeVisitor() = delete;
	SQLiteMakeVisitor(SQL::F2CSQLConn &sql, const SlKernCVS::SupportedConf &supp,
			  const std::string &branch, const std::filesystem::path &base);

	virtual ~SQLiteMakeVisitor() override;
	virtual void ignored(const std::filesystem::path &objPath,
			     const std::string &cond) const override;

	virtual void config(const std::filesystem::path &srcPath,
			    const std::string &cond) const override;

	virtual void configDep(const std::string &parent, const std::string &child) const override;

	virtual void module(const std::filesystem::path &srcPath,
			    const std::filesystem::path &module) const override;
private:
	static bool skipPath(const std::filesystem::path &relPath);

	SQL::F2CSQLConn &sql;
	const SlKernCVS::SupportedConf &supp;
	const std::string branch;
	const std::filesystem::path base;
};

}

#endif // SQLITEMAKEVISITOR_H
