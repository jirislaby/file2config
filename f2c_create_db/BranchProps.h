// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <string>
#include <unordered_map>

#include <sl/helpers/Misc.h>

namespace F2C {

struct BranchProps {
	BranchProps(std::string versionStr) : versionStr(std::move(versionStr)),
		version(SlHelpers::Version::versionSum(BranchProps::versionStr)) { }

	std::string versionStr;
	unsigned version;
};

using BranchesProps = std::unordered_map<std::string, BranchProps>;

}
