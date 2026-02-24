// SPDX-License-Identifier: GPL-2.0-only

#ifndef TREEWALKER_H
#define TREEWALKER_H

#include <any>
#include <filesystem>
#include <functional>
#include <set>
#include <string>
#include <vector>

#include "../parser/make/Parser.h"

namespace MP {
enum EntryType : unsigned int;
}

namespace TW {

class MakeVisitor;

class TreeWalker
{
public:
	using CondStack = std::vector<std::string>;

	TreeWalker() = delete;
	TreeWalker(const std::filesystem::path &start, const MakeVisitor &makeVisitor);

	void walk();

	void addRegularEntry(const CondStack &s, const std::filesystem::path &kbPath,
			     const std::any &interesting, const std::string &cond,
			     const MP::EntryType &type,
			     const std::string &word);
	void addTargetEntry(const CondStack &s,
			    const std::filesystem::path &objPath, const std::string &cond,
			    const MP::EntryType &type,
			    const std::string &entry, bool &found);
private:
	static void forEachSubDir(const std::filesystem::path &dir,
				  const std::function<void (const std::filesystem::path &)> &CB);
	void addDefaultKernelFiles(const CondStack &s, const std::filesystem::path &start);

	bool tryHandleTarget(const CondStack &s, const std::filesystem::path &objPath);
	void handleKbuildFile(const CondStack &s, const std::filesystem::path &kbPath);
	void addDirectory(const std::filesystem::path &kbPath, const CondStack &s,
			  const std::filesystem::path &path);
	void handleObject(const CondStack &s, const std::filesystem::path &objPath,
			  const std::filesystem::path &module);

	static bool isBuiltIn(const std::string &cond);
	static std::string getCond(const CondStack &s);

	MP::Parser parser;
	const MakeVisitor &makeVisitor;
	std::filesystem::path start;
	std::vector<std::string> archs;
	std::vector<std::pair<CondStack, std::filesystem::path>> toWalk;
	std::set<std::filesystem::path> visitedDirs;
	std::set<std::filesystem::path> visitedPaths;
};

}

#endif // TREEWALKER_H
