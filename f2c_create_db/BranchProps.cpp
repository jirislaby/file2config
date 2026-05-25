// SPDX-License-Identifier: GPL-2.0-only

#include <sl/kerncvs/RPMConfig.h>
#include <sl/helpers/Exception.h>

#include "BranchProps.h"

using RunEx = SlHelpers::RuntimeException;
using SlHelpers::raise;

using namespace F2C;

std::string BranchProps::getVerStr(const SlGit::Commit &commit)
{
	auto rpmConf = SlKernCVS::RPMConfig::create(*commit.tree());
	if (!rpmConf)
		RunEx("Cannot obtain a config from ") << std::quoted(commit.idStr()) << ": " <<
							 commit.repo().lastError() << raise;

	auto srcVer = rpmConf->get("SRCVERSION");
	if (!srcVer)
		RunEx("No SRCVERSION in rpm/config.sh of ") << std::quoted(commit.idStr()) <<
			raise;

	return *srcVer;
}

