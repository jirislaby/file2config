// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <any>
#include <filesystem>
#include <functional>
#include <optional>
#include <queue>
#include <unordered_set>
#include <string>
#include <vector>

#include "../parser/make/Parser.h"
#include "../parser/kconfig/Config.h"

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
	TreeWalker(const std::filesystem::path &start, const Kconfig::Config::Configs &configs,
		   const MakeVisitor &makeVisitor);

	void walk();

	void addRegularEntry(CondStack s, const std::filesystem::path &kbPath,
			     const std::any &interesting, const std::string &cond,
			     MP::EntryType type, const std::string &word);
	void addTargetEntry(CondStack s,
			    const std::filesystem::path &objPath, const std::string &cond,
			    const std::string &entry);
private:
	struct ToWalkEntry {
		CondStack cs;
		std::filesystem::path kbPath;
		std::filesystem::path cwd; // kbPath's dir except for make's "include"
	};

	static void forEachSubDir(const std::filesystem::path &dir,
				  const std::function<void (const std::filesystem::path &)> &CB);
	void addDefaultKernelFiles(CondStack s, const std::filesystem::path &start);

	bool tryHandleTarget(CondStack s, const std::filesystem::path &objPath);
	void handleKbuildFile(const ToWalkEntry &e);
	void addDirectory(const std::filesystem::path &kbPath, CondStack s,
			  const std::filesystem::path &path);
	void handleObject(CondStack s, const std::filesystem::path &objPath,
			  const std::filesystem::path &module);

	static bool isBuiltIn(const std::string &cond);
	static std::optional<std::string> getCond(const CondStack &s);
	std::optional<std::string> getTristateConf(const CondStack &s);

	void appendToWalk(CondStack s, std::filesystem::path kbPath,
			  std::filesystem::path cwd = {});

	MP::Parser parser;
	const Kconfig::Config::Configs &m_configs;
	const MakeVisitor &makeVisitor;

	std::filesystem::path start;
	std::vector<std::string> archs;
	std::queue<ToWalkEntry> m_toWalk;
	std::unordered_set<std::filesystem::path> m_visitedMakefiles;
	std::unordered_set<std::filesystem::path> visitedDirs;
	std::unordered_set<std::filesystem::path> visitedPaths;
};

}

