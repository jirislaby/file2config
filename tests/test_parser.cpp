// SPDX-License-Identifier: GPL-2.0-only

#include <cassert>
#include <iostream>
#include <set>

#include <sl/helpers/Color.h>
#include <string_view>

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

	static const struct {
		std::string_view cond;
		std::string_view rhs;
		Entry expected;
	} data[] = {
		{ "y",			"mod-y.o", { "y", "mod-y.o" } },
		{ "$(CONFIG_ABC)",	"mod-abc.o", { "CONFIG_ABC", "mod-abc.o" } },
		{ "y",			"$(VAR).o", { "y", "mod-var.o" } },
		{ "y",			"$(src)/mod-src.o", { "y", "/src/mod-src.o" } },
		{ "y",			"$(srctree)/mod-tree.o", { "y", "/srctree/mod-tree.o" } },
		{ "y$(CONFIG_MMU_SUN3)","dma.o", { "CONFIG_MMU_SUN3", "dma.o" } },
	};

	std::stringstream ss;
	ss <<	"VAR := mod-var\n"
		"VAR2 += content\n";
	for (const auto &e : data) {
		ss << "obj-" << e.cond << " := " << e.rhs << "\n";
	}

	assert(parser.parse(ss.view()));

	EntryCont cont;

	class TestVisitor : public MP::EntryVisitor {
	public:
		TestVisitor(EntryCont &cont) : cont(cont) {}
		~TestVisitor() {
			assert(m_set == 2);
		}

		virtual const std::any isInteresting(const std::string &) const override {
			return true;
		}

		virtual void entry(const std::any &, const std::string &cond,
				   MP::EntryType type, const std::string &word) const override {
			assert(type == MP::EntryType::Object);
			cont.insert(std::make_pair(cond, word));
		}

		virtual std::vector<std::string> getVariable(const std::string &id) const override {
			if (id == "VAR")
				return { "mod-var" };
			return {};
		}

		virtual void setVariable(const std::string &id, bool reset,
					 const std::string &val) const override {
			if (id == "VAR") {
				const_cast<TestVisitor *>(this)->m_set++;
				assert(reset);
				assert(val == "mod-var");
			} else if (id == "VAR2") {
				const_cast<TestVisitor *>(this)->m_set++;
				assert(!reset);
				assert(val == "content");
			}
		}

		EntryCont &cont;
		unsigned m_set = 0;
	} visitor(cont);

	parser.walkAST({}, visitor, "/srctree", "/src");

	Clr(std::cerr) << "data:";
	for (const auto &e : data)
		Clr(std::cerr) << "\tcond=" << e.expected.first << " mod=" << e.expected.second;

	Clr(std::cerr) << "found:";
	for (const auto &e : cont)
		Clr(std::cerr) << "\tcond=" << e.first << " mod=" << e.second;

	for (const auto &e : data)
		assert(cont.find(e.expected) != cont.end());
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

