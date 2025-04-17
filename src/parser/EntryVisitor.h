// SPDX-License-Identifier: GPL-2.0-only

#ifndef ENTRYCALLBACK_H
#define ENTRYCALLBACK_H

#include <any>
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
			   const enum EntryType &type, const std::string &word) const = 0;
};

}

#endif
