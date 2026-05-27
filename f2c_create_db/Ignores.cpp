// SPDX-License-Identifier: GPL-2.0-only

#include <fnmatch.h>
#include <nlohmann/json.hpp>

#include <sl/helpers/Exception.h>

#include "Ignores.h"

using RunEx = SlHelpers::RuntimeException;
using SlHelpers::raise;

using namespace F2C;

void Ignores::processOne(F2CSQLConn &sql, const std::string &branch,
			 const std::vector<Json> &patterns, const std::filesystem::path &relPath)
{
	for (const auto &pattern: patterns)
		if (!fnmatch(pattern.get_ref<const Json::string_t &>().c_str(),
			     relPath.c_str(), FNM_PATHNAME)) {
			const auto dirFile = sql.insertPath(relPath);
			if (!dirFile || !sql.insertIFBMap(branch, dirFile->first, dirFile->second))
				RunEx("Cannot insert ignore: ") << sql.lastError() << raise;
		}
}

void Ignores::process(F2CSQLConn &sql, const std::string &branch, const Json &json,
		      const std::filesystem::path &root)
{
	if (!json.contains("ignored_files"))
		return;
	const auto ignoredFiles = json["ignored_files"];
	const auto allIt = ignoredFiles.find("all");
	const auto all = (allIt != ignoredFiles.end()) ?
		&allIt->get_ref<const Json::array_t &>() : nullptr;

	const auto forBranchIt = ignoredFiles.find(branch);
	const auto forBranch = (forBranchIt != ignoredFiles.end()) ?
				&forBranchIt->get_ref<const Json::array_t &>() : nullptr;

	for (const auto &e: std::filesystem::recursive_directory_iterator(root)) {
		if (!e.is_regular_file())
			continue;

		const auto relPath = e.path().lexically_relative(root);

		if (all)
			processOne(sql, branch, *all, relPath);
		if (forBranch)
			processOne(sql, branch, *forBranch, relPath);
	}
}
