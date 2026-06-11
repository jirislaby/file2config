// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <any>
#include <filesystem>
#include <functional>
#include <optional>
#include <queue>
#include <set>
#include <unordered_set>
#include <string>
#include <vector>

#include "../Configs.h"
#include "../parser/make/Parser.h"
#include "../parser/kconfig/Config.h"
#include "SQLiteMakeVisitor.h"

namespace SlKernCVS {
class SupportedConf;
enum class SupportState;
}

namespace MP {
enum EntryType : unsigned int;
}

namespace TW {

class TreeWalker
{
public:
	using CondStack = std::vector<std::string>;

	TreeWalker() = delete;
	TreeWalker(F2C::F2CSQLConn &sql, const SlKernCVS::SupportedConf &supp,
		   const std::string &branch, const std::filesystem::path &start,
		   const Kconfig::Config::Configs &configs,
		   const F2C::EnabledConfigMap &enabledConfigs);

	void walk();

	void addRegularEntry(CondStack s, const std::filesystem::path &kbPath,
			     const std::any &interesting, std::string cond,
			     MP::EntryType type, const std::string &word);
	void addTargetEntry(CondStack s, const std::filesystem::path &objPath, std::string cond,
			    const std::string &entry);
private:
	using PathSet = std::unordered_set<std::filesystem::path>;

	struct ToWalkEntry {
		CondStack cs;
		std::filesystem::path kbPath;
		std::filesystem::path cwd; // kbPath's dir except for make's "include"
	};

	auto startRelative(const std::filesystem::path &path) const {
		return path.lexically_relative(start).lexically_normal();
	}

	std::vector<std::string> getVariable(const std::string &id) const;

	static bool skipPath(const std::filesystem::path &relPath);

	static void forEachSubDir(const std::filesystem::path &dir,
				  const std::function<void (const std::filesystem::path &)> &CB);
	void addDefaultKernelFiles(CondStack s, const std::filesystem::path &start);

	bool tryHandleTarget(CondStack s, const std::filesystem::path &objPath);
	void handleKbuildFile(ToWalkEntry &&e);
	void addDirectory(const std::filesystem::path &kbPath, CondStack s,
			  const std::filesystem::path &path);
	void handleCSource(const std::string &cond,
			   std::filesystem::path &&srcPath,
			   const std::filesystem::path &relModule,
			   SlKernCVS::SupportState supported);
	void handleObject(CondStack &&s, std::filesystem::path &&objPath,
			  std::filesystem::path &&module);

	static bool isBuiltIn(const std::string &cond);
	static std::optional<std::string> getCond(const CondStack &s);
	std::optional<std::string> getTristateConf(const CondStack &s);

	void appendToWalk(CondStack s, std::filesystem::path kbPath,
			  std::filesystem::path cwd = {});

	MP::Parser parser;
	std::unordered_multimap<std::string, std::string> m_vars;
	const SlKernCVS::SupportedConf &m_supp;
	const Kconfig::Config::Configs &m_configs;
	const F2C::EnabledConfigMap &m_enabledConfigs;
	const SQLiteMakeVisitor m_makeVisitor;

	std::filesystem::path start;
	std::vector<std::string> archs;
	std::queue<ToWalkEntry> m_toWalk;
	PathSet m_skipMakefiles;
	std::set<std::pair<std::filesystem::path, CondStack>> m_visitedMakefiles;
};

}

