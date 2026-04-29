// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <any>
#include <filesystem>
#include <string>
#include <vector>

namespace MP {

enum EntryType : unsigned int {
	Directory,
	Object,
};

class EntryVisitor {
public:
	virtual std::any isInteresting(const std::string &lhs) const = 0;

	virtual void entry(const std::any &interesting, const std::string &cond,
			   EntryType type, std::string &&word) const = 0;

	virtual void include(std::filesystem::path &&/*dest*/) const {}

	virtual std::vector<std::string> getVariable(const std::string &id) const = 0;
	virtual void setVariable(const std::string &/*id*/, bool /*reset*/,
				 const std::string &/*val*/) const {}
};

}

