// SPDX-License-Identifier: GPL-2.0-only

#include "MakeParserExprListener.h"
#include "Parser.h"

using namespace MP;

void Parser::walkAST(const std::vector<std::string> &archs, const EntryVisitor &entryVisitor,
		     const std::filesystem::path &rootDir, const std::filesystem::path &curDir)
{
	antlr4::tree::ParseTreeWalker walker;
	MakeExprListener l{ archs, entryVisitor, rootDir, curDir };
	walker.walk(&l, m_tree);
}

antlr4::ParserRuleContext *Parser::getTree()
{
	return m_parser->makefile();
}
