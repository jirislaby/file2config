// SPDX-License-Identifier: GPL-2.0-only

#include "MakeParserExprListener.h"
#include "Parser.h"

using namespace MP;

void Parser::walkTree(const std::vector<std::string> &archs, const EntryVisitor &entryVisitor)
{
	antlr4::tree::ParseTreeWalker walker;
	MakeExprListener l{ archs, entryVisitor };
	walker.walk(&l, m_tree);
}

antlr4::ParserRuleContext *Parser::getTree()
{
	return m_parser->makefile();
}
