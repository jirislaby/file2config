#ifndef TREEWALKER_H
#define TREEWALKER_H

#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "../parser/parser.h"

namespace TW {

class TreeWalker
{
public:
	TreeWalker() = delete;
	TreeWalker(const std::filesystem::path &start);

	void walk();

private:
	using CondStack = std::vector<std::string>;

	bool tryHandleTarget(const CondStack &s, const std::filesystem::path &objPath);
	void handleObject(const CondStack &s, const std::filesystem::path &objPath);
	void handleKbuildFile(const CondStack &s, const std::filesystem::path &kbPath);
	void walkKbuild(const CondStack &s, const std::filesystem::path &path);

	static bool isBuiltIn(const std::string &cond);
	static std::string getCond(const CondStack &s);

	MP::Parser parser;
	std::vector<std::string> archs;
	std::vector<std::pair<CondStack, std::filesystem::path>> toWalk;
	std::set<std::filesystem::path> visited;
};

}

#endif // TREEWALKER_H
