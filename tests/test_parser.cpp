// SPDX-License-Identifier: GPL-2.0-only

#include <cassert>
#include <iostream>
#include <set>

#include <sl/helpers/Color.h>

#include "kconfig/Parser.h"
#include "make/EntryVisitor.h"
#include "make/Parser.h"

using Clr = SlHelpers::Color;

namespace {

void testVisitor()
{
	Clr(std::cerr, Clr::GREEN) << __func__;

	MP::Parser parser;

	using Entry = std::pair<std::string, std::string>;
	using EntryCont = std::set<Entry>;

	static const std::pair<std::string, Entry> data[] = {
		{ "y",			{ "y", "mod-y.o" } },
		{ "$(CONFIG_ABC)",	{ "CONFIG_ABC", "mod-abc.o" } },
	};

	std::stringstream ss;
	for (const auto &e : data) {
		ss << "obj-" << e.first << " := " << e.second.second << "\n";
	}

	assert(parser.parse(ss.view()));

	EntryCont cont;

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

	parser.walkAST({}, visitor);

	std::cerr << "data:\n";
	for (const auto &e : data)
		std::cerr << "\tcond=" << e.second.first << " mod=" << e.second.second << "\n";

	std::cerr << "found:\n";
	for (const auto &e : cont)
		std::cerr << "\tcond=" << e.first << " mod=" << e.second << "\n";

	for (const auto &e : data)
		assert(cont.find(e.second) != cont.end());
}

void testMakefile(const std::filesystem::path &makefile)
{
	Clr(std::cerr, Clr::GREEN) << "Tesing " << makefile.filename();

	assert(MP::Parser().parse(makefile));
}

void testMakefiles(const std::filesystem::path &makefiles)
{
	Clr(std::cerr, Clr::GREEN) << __func__;

	std::error_code ec;

	const auto iter = std::filesystem::directory_iterator{makefiles, ec};
	assert(!ec);

	for (const auto &entry : iter)
		if (entry.is_regular_file() && entry.path().stem().string() == "Makefile")
			testMakefile(entry.path());
}

void testKconfig()
{
	Clr(std::cerr, Clr::GREEN) << __func__;

	Kconfig::Parser p;
	static constinit std::string_view kconf(
		"config ABC\n"
		"tristate \"some desc\"\n"
		"\tdepends on XYZ\n"
		"\thelp\n"
		" Some text\n"
		"config DEF\n"
		"bool\n"
	);

	assert(p.parse(kconf, false));

	std::unordered_map<std::string, Kconfig::ConfType> configs;
	p.walkConfigs([&configs](auto conf, auto type) {
		configs.emplace(std::move(conf), type);
	});
	assert(configs.find("ABC") != configs.end());
	assert(configs.find("DEF") != configs.end());
	assert(configs["ABC"] == Kconfig::ConfType::Tristate);
	assert(configs["DEF"] == Kconfig::ConfType::Bool);
}

} // namespace

#ifndef TESTS_DIR
#define TESTS_DIR	"."
#endif

int main()
{
	std::filesystem::path tests{TESTS_DIR};

	Clr(std::cerr) << "Tests dir: " << tests;

	testVisitor();
	testMakefiles(tests/"makefiles");

	testKconfig();

	return 0;
}

