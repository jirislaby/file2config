// SPDX-License-Identifier: GPL-2.0-only

#include <iostream>
#include <unordered_set>

#include <sl/helpers/Color.h>
#include <sl/helpers/Exception.h>
#include <sl/kerncvs/SupportedConf.h>

#include "../F2CSQLConn.h"
#include "../Verbose.h"

#include "SQLiteMakeVisitor.h"

using namespace TW;

using Clr = SlHelpers::Color;
using RunEx = SlHelpers::RuntimeException;
using SlHelpers::raise;

SQLiteMakeVisitor::SQLiteMakeVisitor(F2C::F2CSQLConn &sql, const SlKernCVS::SupportedConf &supp,
				     const std::string &branch,
				     const Kconfig::Config::Configs &configs) :
	sql(sql), supp(supp), branch(branch), m_configs(configs)
{
}

SQLiteMakeVisitor::~SQLiteMakeVisitor()
{
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
	if (skipPath(srcPath))
		return;

	if (F2C::verbose > 1)
		std::cout << "SQL " << cond << " " << srcPath.string() << "\n";

	if (!m_configs.contains(cond)) {
		if (F2C::verbose > 0)
			Clr(std::cerr, Clr::YELLOW) << srcPath << " depends on \"" << cond <<
						       "\", but that is not defined!";
		return;
	}

	auto dirFile = sql.insertPath(srcPath);
	if (!dirFile || !sql.insertCFMap(branch, cond, std::move(dirFile->first),
					 std::move(dirFile->second)))
		RunEx("cannot insert CFMap: ") << sql.lastError() << raise;
}

void SQLiteMakeVisitor::module(const std::filesystem::path &srcPath,
			       const std::filesystem::path &module,
			       const std::optional<std::string> &moduleConf) const
{
	if (skipPath(srcPath))
		return;

	if (F2C::verbose > 1)
		std::cout << "SQL MOD " << module.string() << ' ' << srcPath.string() << ' ' <<
			  (moduleConf ? *moduleConf : "NULL") << '\n';

	auto dirMod = module.parent_path();
	auto fileMod = module.filename();
	auto supported = supp.supportState(module);
	auto dirFile = sql.insertPath(srcPath);
	if (!dirFile || !sql.insertDir(dirMod) ||
			!sql.insertModule(dirMod, fileMod, moduleConf) ||
			!sql.insertMDMap(branch, dirMod, fileMod, supported) ||
			!sql.insertMFMap(branch, dirMod, fileMod, std::move(dirFile->first),
					 std::move(dirFile->second)))
		RunEx("cannot insert module maps: ") << sql.lastError() << raise;
}
