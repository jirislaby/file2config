// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <set>
#include <string>
#include <string_view>
#include <unordered_map>

#include <sl/git/Repo.h>
#include <sl/helpers/String.h>

#include "sql/F2CSQLConn.h"

#include "BranchProps.h"

namespace F2C {

class Renames {
public:
	Renames() = delete;

	static void processRenames(SQL::F2CSQLConn &sql, const SlGit::Repo &lrepo,
				   const BranchesProps &branchesProps);

private:
	struct RenameInfo {
		std::string path;
		unsigned similarity;
	};

        using RenameMap = std::unordered_map<std::string, RenameInfo, SlHelpers::String::Hash,
	      SlHelpers::String::Eq>;

	static void processRenamesBetween(SQL::F2CSQLConn &sql, const SlGit::Repo &lrepo,
					  const BranchProps &begin, std::string_view end,
					  RenameMap &renames);

	static auto getUniqTags(const BranchesProps &branchesProps) {
		std::set<BranchProps, decltype([](const auto &e1, const auto &e2) {
					       return e1.version < e2.version;
					       })> ret;
		for (const auto &[branch, prop]: branchesProps)
			ret.insert(prop);
		return ret;
	}
};

} // namespace
