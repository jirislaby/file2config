// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <string>
#include <unordered_map>

#include <sl/git/Commit.h>
#include <sl/helpers/Misc.h>

namespace F2C {

struct BranchProps {
	BranchProps(std::string versionStr) : versionStr(std::move(versionStr)),
		version(SlHelpers::Version::versionSum(BranchProps::versionStr)) { }
	BranchProps(const SlGit::Commit &commit) : BranchProps(getVerStr(commit)) { };

	std::string versionStr;
	unsigned version;
private:
	static std::string getVerStr(const SlGit::Commit &commit);
};

using BranchesProps = std::unordered_map<std::string, BranchProps>;

}
