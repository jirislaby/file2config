// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <string>
#include <vector>

#include "../Parser.h"

class MakeLexer;
class MakeParser;

namespace MP {

class EntryVisitor;

class Parser : public Parsers::Parser<MakeLexer, MakeParser> {
public:
	void walkTree(const std::vector<std::string> &archs, const EntryVisitor &entryVisitor);
protected:
	virtual antlr4::ParserRuleContext *getTree();
};

}
