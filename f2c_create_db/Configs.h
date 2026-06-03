// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <string>
#include <unordered_map>

#include <sl/kerncvs/CollectConfigs.h>

namespace F2C {

using EnabledConfigMap = std::unordered_map<std::string, SlKernCVS::ConfigValue>;

constexpr int getConfValWeight(SlKernCVS::ConfigValue conf)
{
	switch (conf) {
	case SlKernCVS::ConfigValue::Disabled:
		return 0;
	case SlKernCVS::ConfigValue::Module:
		return 1;
	case SlKernCVS::ConfigValue::BuiltIn:
		return 2;
	case SlKernCVS::ConfigValue::WithValue:
		break;
	}

	return -1;
}

} // namespace

namespace SlKernCVS {

inline bool operator<(ConfigValue lhs, ConfigValue rhs)
{
	return F2C::getConfValWeight(lhs) < F2C::getConfValWeight(rhs);
}

} // namespace
