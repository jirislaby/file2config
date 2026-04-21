// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <any>
#include <filesystem>
#include <string>

namespace MP {

enum EntryType : unsigned int {
	Directory,
	Object,
};

class EntryVisitor {
public:
	virtual const std::any isInteresting(const std::string &lhs) const = 0;

	virtual void entry(const std::any &interesting, const std::string &cond,
			   EntryType type, const std::string &word) const = 0;

	virtual void include(const std::filesystem::path &/*dest*/) const {}
};

}

