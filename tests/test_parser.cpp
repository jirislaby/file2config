// SPDX-License-Identifier: GPL-2.0-only

#include <cassert>
#include <iostream>
#include <set>

#include "EntryVisitor.h"
#include "Parser.h"

int main()
{
	MP::Parser parser;

	using Entry = std::pair<std::string, std::string>;
	using EntryCont = std::set<Entry>;

	static const std::pair<std::string, Entry> data[] = {
		{ "y",			{ "y", "mod-y.o" } },
		{ "$(CONFIG_ABC)",	{ "CONFIG_ABC", "mod-abc.o" } },
	};

	EntryCont cont;

	std::stringstream ss;
	for (const auto &e : data) {
		ss << "obj-" << e.first << " := " << e.second.second << "\n";
	}


	class TestVisitor : public MP::EntryVisitor {
	public:
		TestVisitor(EntryCont &cont) : cont(cont) {}

		virtual const std::any isInteresting(const std::string &) const {
			return true;
		}

		virtual void entry(const std::any &, const std::string &cond,
				   const enum MP::EntryType &type, const std::string &word) const {
			assert(type == MP::EntryType::Object);
			cont.insert(std::make_pair(cond, word));
		}

		EntryCont &cont;
	} visitor(cont);

	parser.parse({}, ss.str(), visitor);

	std::cout << "data:\n";
	for (const auto &e : data)
		std::cout << "\tcond=" << e.second.first << " mod=" << e.second.second << "\n";

	std::cout << "found:\n";
	for (const auto &e : cont)
		std::cout << "\tcond=" << e.first << " mod=" << e.second << "\n";

	for (const auto &e : data)
		assert(cont.find(e.second) != cont.end());

	return 0;
}

