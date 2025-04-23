// SPDX-License-Identifier: GPL-2.0-only

#include "../Verbose.h"
#include "../sql/SQLConn.h"

#include "SQLiteMakeVisitor.h"

using namespace TW;

SQLiteMakeVisitor::SQLiteMakeVisitor(SQL::SQLConn &sql, const std::string &branch,
				     const std::filesystem::path &base) :
	sql(sql), branch(branch), base(base)
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

void SQLiteMakeVisitor::config(const std::filesystem::path &srcPath,
			       const std::string &cond) const
{
	if (srcPath.extension() != ".c")
		return;
	auto relPath = srcPath.lexically_relative(base);

	static const std::string avoidPath[] = { "Documentation", "samples", "tools", };
	auto first = *relPath.begin();
	if (F2C::verbose > 1)
		std::cout << "SQL " << cond << " " << srcPath.string() << "\n";
	for (const auto &avoid : avoidPath) {
		if (avoid == first)
			return;
	}
	auto dir = relPath.parent_path();
	auto file = relPath.filename();
	if (sql.insertConfig(cond))
		return;
	if (sql.insertDir(dir))
		return;
	if (sql.insertFile(dir, file))
		return;
	if (sql.insertCFMap(branch, cond, dir, file))
		return;
}
