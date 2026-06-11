// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <filesystem>
#include <string>

namespace SlKernCVS {
enum class SupportState;
}

namespace F2C {
class F2CSQLConn;
}

namespace TW {

class SQLiteMakeVisitor {
public:
	SQLiteMakeVisitor() = delete;
	SQLiteMakeVisitor(F2C::F2CSQLConn &sql, const std::string &branch) :
		sql(sql), branch(branch) {}

	~SQLiteMakeVisitor() {}

	void config(const std::filesystem::path &srcPath, const std::string &cond) const;

	void module(const std::filesystem::path &module,
		    const std::string &moduleConf,
		    SlKernCVS::SupportState supported) const;

	void moduleFile(const std::filesystem::path &srcPath,
			const std::filesystem::path &module) const;
private:
	F2C::F2CSQLConn &sql;
	const std::string branch;
};

}
