// SPDX-License-Identifier: GPL-2.0-only

#ifndef MAKEVISITOR_H
#define MAKEVISITOR_H

#include <filesystem>
#include <string>

namespace TW {

class MakeVisitor
{
public:
	virtual ~MakeVisitor() {}

	virtual void ignored(const std::filesystem::path &objPath, const std::string &cond) const = 0;
	virtual void config(const std::filesystem::path &srcPath, const std::string &cond) const = 0;
	virtual void module(const std::filesystem::path &srcPath,
			    const std::filesystem::path &module) const = 0;
private:
};

}

#endif // MAKEVISITOR_H
