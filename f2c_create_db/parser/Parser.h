// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <filesystem>
#include <memory>

namespace antlr4 {
class ANTLRInputStream;
class CommonTokenStream;
class ParserRuleContext;
}

namespace Parsers {

template <class ALexer, class AParser>
class Parser {
public:
	Parser();
	~Parser();

	bool parse(std::string_view str, bool trySLL = true);
	bool parse(const std::filesystem::path &file, bool trySLL = true);
	void reset();

protected:
	bool parseSLL();
	bool parseLL();
	bool parse(antlr4::ANTLRInputStream &is, bool trySLL);

	virtual antlr4::ParserRuleContext *getTree() = 0;

	antlr4::ParserRuleContext *m_tree;
	std::unique_ptr<antlr4::ANTLRInputStream> m_input;
	std::unique_ptr<ALexer> m_lexer;
	std::unique_ptr<antlr4::CommonTokenStream> m_tokens;
	std::unique_ptr<AParser> m_parser;
};

}
