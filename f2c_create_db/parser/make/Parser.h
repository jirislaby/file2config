// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "../Parser.h"

class MakeLexer;
class MakeParser;

namespace MP {

class EntryVisitor;

class Parser : public Parsers::Parser<MakeLexer, MakeParser> {
public:
	void walkAST(const std::vector<std::string> &archs, const EntryVisitor &entryVisitor,
		     const std::filesystem::path &rootDir, const std::filesystem::path &curDir);
protected:
	virtual antlr4::ParserRuleContext *getTree();
};

}
