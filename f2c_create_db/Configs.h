// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <string>
#include <unordered_map>

namespace SlKernCVS {
enum class ConfigValue : char;
}

namespace F2C {

using EnabledConfigMap = std::unordered_map<std::string, SlKernCVS::ConfigValue>;

} // namespace
