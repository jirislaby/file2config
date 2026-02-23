// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

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

class ConfigRange {
public:
	struct iterator {
		unsigned v;

		ConfType operator*() const { return static_cast<ConfType>(v); }

		iterator &operator++() {
			++v;
			return *this;
		}

		bool operator==(const iterator &other) const { return v == other.v; }
		bool operator!=(const iterator &other) const { return v != other.v; }
	};

	iterator begin() const { return { 0 }; }
	iterator end() const { return { static_cast<unsigned>(ConfType::Count) }; }
};

} // namespace
