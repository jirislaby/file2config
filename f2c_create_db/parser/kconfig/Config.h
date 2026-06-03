// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

#include <sl/helpers/Enum.h>

namespace Kconfig {

#define CONFIG_TYPES(X) \
	X(Unknown) \
	X(Bool) \
	X(Tristate) \
	X(DefBool) \
	X(DefTristate) \
	X(Int) \
	X(Hex) \
	X(String) \
	X(Range)

enum class ConfType : unsigned {
#define EXP(x) x,
	CONFIG_TYPES(EXP)
#undef EXP
	Count,
};

class Config {
public:
	Config() = delete;

	using Configs = std::unordered_map<std::string, ConfType>;

	static constexpr std::string_view getName(ConfType ct) noexcept {
		switch (ct) {
#define EXP(x) case ConfType::x: return #x;
		CONFIG_TYPES(EXP)
#undef EXP
		case ConfType::Count:
			break;
		}
		return "INVALID";
	}
};

using ConfigRange = SlHelpers::EnumRange<ConfType>;

} // namespace
