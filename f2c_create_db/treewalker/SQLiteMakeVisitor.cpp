// SPDX-License-Identifier: GPL-2.0-only

#include <iostream>

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

void SQLiteMakeVisitor::config(const std::filesystem::path &srcPath,
			       const std::string &cond) const
{
	if (F2C::verbose > 1)
		std::cout << "SQL " << cond << " " << srcPath.string() << "\n";

	auto dirFile = sql.insertPath(srcPath);
	if (!dirFile || !sql.insertCFMap(branch, cond, std::move(dirFile->first),
					 std::move(dirFile->second)))
		RunEx("cannot insert CFMap: ") << sql.lastError() << raise;
}

void SQLiteMakeVisitor::module(const std::filesystem::path &module,
			       const std::string &moduleConf,
			       SlKernCVS::SupportState supported) const
{
	if (F2C::verbose > 1)
		Clr() << "SQL MOD " << module.string() << ' ' << moduleConf;

	auto dirMod = module.parent_path();
	auto fileMod = module.filename();
	if (!sql.insertDir(dirMod) ||
			!sql.insertModule(dirMod, fileMod, moduleConf) ||
			!sql.insertMDMap(branch, dirMod, fileMod, static_cast<int>(supported)))
		RunEx("cannot insert module maps: ") << sql.lastError() << raise;
}

void SQLiteMakeVisitor::moduleFile(const std::filesystem::path &srcPath,
				   const std::filesystem::path &module) const
{
	if (F2C::verbose > 1)
		Clr() << "SQL MOD FILE " << module.string() << ' ' << srcPath.string();

	auto dirMod = module.parent_path();
	auto fileMod = module.filename();
	auto dirFile = sql.insertPath(srcPath);
	if (!dirFile || !sql.insertMFMap(branch, dirMod, fileMod, std::move(dirFile->first),
					 std::move(dirFile->second)))
		RunEx("cannot insert module file map: ") << sql.lastError() << raise;
}
