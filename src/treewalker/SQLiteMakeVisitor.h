// SPDX-License-Identifier: GPL-2.0-only

#ifndef SQLITEMAKEVISITOR_H
#define SQLITEMAKEVISITOR_H

#include <iostream>

#include "MakeVisitor.h"

namespace SQL {
class SQLConn;
}

namespace TW {

class SQLiteMakeVisitor : public MakeVisitor
{
public:
	SQLiteMakeVisitor() = delete;
	SQLiteMakeVisitor(SQL::SQLConn &sql, const std::string &branch,
			  const std::filesystem::path &base);

	virtual ~SQLiteMakeVisitor() override;
	virtual void ignored(const std::filesystem::path &objPath,
			     const std::string &cond) const override;

	virtual void config(const std::filesystem::path &srcPath,
			    const std::string &cond) const override;
private:
	SQL::SQLConn &sql;
	const std::string branch;
	const std::filesystem::path base;
};

}

#endif // SQLITEMAKEVISITOR_H
