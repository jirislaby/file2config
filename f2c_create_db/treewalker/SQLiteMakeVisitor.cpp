// SPDX-License-Identifier: GPL-2.0-only

#include <iostream>
#include <unordered_set>

#include <sl/helpers/Color.h>
#include <sl/kerncvs/SupportedConf.h>

#include "../Verbose.h"
#include "../sql/F2CSQLConn.h"

#include "SQLiteMakeVisitor.h"

using namespace TW;

using Clr = SlHelpers::Color;

SQLiteMakeVisitor::SQLiteMakeVisitor(SQL::F2CSQLConn &sql, const SlKernCVS::SupportedConf &supp,
				     const std::string &branch, const std::filesystem::path &base) :
	sql(sql), supp(supp), branch(branch), base(base)
{
}

SQLiteMakeVisitor::~SQLiteMakeVisitor()
{
}

void SQLiteMakeVisitor::ignored(const std::filesystem::path &objPath, const std::string &cond) const
{
	if (F2C::verbose > 1)
		std::cout << "ignoring already reported " << objPath << ", now with " << cond << '\n';
}

bool SQLiteMakeVisitor::skipPath(const std::filesystem::path &relPath)
{
	if (relPath.extension() != ".c")
		return true;

	static const std::unordered_set<std::string_view> skipPaths {
		"Documentation",
		"samples",
		"tools",
	};

	const auto first = relPath.begin()->string();
	return skipPaths.contains(first);
}

void SQLiteMakeVisitor::config(const std::filesystem::path &srcPath,
			       const std::string &cond) const
{
	auto relPath = srcPath.lexically_relative(base);

	if (skipPath(relPath))
		return;

	if (F2C::verbose > 1)
		std::cout << "SQL " << cond << " " << relPath.string() << "\n";

	auto dirFile = sql.insertPath(relPath);
	if (!dirFile || !sql.insertConfig(cond) ||
			!sql.insertCFMap(branch, cond, std::move(dirFile->first),
					 std::move(dirFile->second)))
		Clr(std::cerr, Clr::RED) << "cannot insert CFMap: " << sql.lastError();
}

void SQLiteMakeVisitor::module(const std::filesystem::path &srcPath,
			       const std::filesystem::path &module) const
{
	auto relPath = srcPath.lexically_relative(base);
	auto relMod = module.lexically_relative(base);

	if (skipPath(relPath))
		return;

	if (F2C::verbose > 1)
		std::cout << "SQL MOD " << relMod.string() << " " << relPath.string() << "\n";

	auto dirMod = relMod.parent_path();
	auto fileMod = relMod.filename();
	auto supported = supp.supportState(relMod);
	auto dirFile = sql.insertPath(relPath);
	if (!dirFile || !sql.insertDir(dirMod) ||
			!sql.insertModule(dirMod, fileMod) ||
			!sql.insertMDMap(branch, dirMod, fileMod, supported) ||
			!sql.insertMFMap(branch, dirMod, fileMod, std::move(dirFile->first),
					 std::move(dirFile->second)))
		Clr(std::cerr, Clr::RED) << "cannot insert module maps: " << sql.lastError();
}
