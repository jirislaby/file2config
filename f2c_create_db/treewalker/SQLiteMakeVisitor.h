// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace SlKernCVS {
class SupportedConf;
}

namespace F2C {
class F2CSQLConn;
}

namespace TW {

class SQLiteMakeVisitor {
public:
	SQLiteMakeVisitor() = delete;
	SQLiteMakeVisitor(F2C::F2CSQLConn &sql, const SlKernCVS::SupportedConf &supp,
			  const std::string &branch) :
		sql(sql), supp(supp), branch(branch) {}

	~SQLiteMakeVisitor() {}

	void config(const std::filesystem::path &srcPath, const std::string &cond) const;

	void module(const std::filesystem::path &srcPath,
		    const std::filesystem::path &module,
		    const std::optional<std::string> &moduleConf) const;
private:
	F2C::F2CSQLConn &sql;
	const SlKernCVS::SupportedConf &supp;
	const std::string branch;
};

}
