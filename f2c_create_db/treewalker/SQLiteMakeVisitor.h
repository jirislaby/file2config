// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include "MakeVisitor.h"
#include "../parser/kconfig/Config.h"

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
			  const std::string &branch, const std::filesystem::path &base,
			  const Kconfig::Config::Configs &configs);

	virtual ~SQLiteMakeVisitor() override;
	virtual void ignored(const std::filesystem::path &objPath,
			     const std::string &cond) const override;

	virtual void config(const std::filesystem::path &srcPath,
			    const std::string &cond) const override;

	virtual void configDep(const std::string &parent, const std::string &child) const override;

	virtual void module(const std::filesystem::path &srcPath,
			    const std::filesystem::path &module,
			    const std::optional<std::string> &moduleConf) const override;
private:
	static bool skipPath(const std::filesystem::path &relPath);

	SQL::F2CSQLConn &sql;
	const SlKernCVS::SupportedConf &supp;
	const std::string branch;
	const std::filesystem::path base;
	const Kconfig::Config::Configs &m_configs;
};

}
