// SPDX-License-Identifier: GPL-2.0-only

#ifndef PARSER_H
#define PARSER_H

#include <memory>
#include <string>
#include <vector>

class MakeLexer;
class MakeParser;

namespace antlr4 {
class ANTLRInputStream;
class CommonTokenStream;
class ParserRuleContext;
}

namespace MP {

class EntryCallback;

class Parser
{
public:
	Parser();
	~Parser();

	int parse(const std::vector<std::string> &archs, const std::string &file,
		  const EntryCallback &CB);
	void reset();

	void walkTree(const EntryCallback &CB);
private:
	std::vector<std::string> archs;
	antlr4::ParserRuleContext *tree;
	std::unique_ptr<antlr4::ANTLRInputStream> input;
	std::unique_ptr<MakeLexer> lexer;
	std::unique_ptr<antlr4::CommonTokenStream> tokens;
	std::unique_ptr<MakeParser> parser;
};

}

#endif // PARSER_H
