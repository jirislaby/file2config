// SPDX-License-Identifier: GPL-2.0-only

#include "KconfigParser.h"
#include "KconfigParserConfigListener.h"
#include "Parser.h"

using namespace Kconfig;

void Parser::walkConfigs(ConfigCB configCB) const
{
	antlr4::tree::ParseTreeWalker walker;
	KconfigParserConfigListener l{ std::move(configCB) };
	walker.walk(&l, m_tree);
}

antlr4::ParserRuleContext *Parser::getTree()
{
	return m_parser->kbuild();
}
