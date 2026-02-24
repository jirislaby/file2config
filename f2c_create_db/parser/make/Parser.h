// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <filesystem>
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

class EntryVisitor;

class Parser
{
public:
	Parser();
	~Parser();

	int parse(const std::vector<std::string> &archs, const std::string &str,
		  const EntryVisitor &entryVisitor);
	int parse(const std::vector<std::string> &archs, const std::filesystem::path &file,
		  const EntryVisitor &entryVisitor);
	void reset();

	void walkTree(const EntryVisitor &entryVisitor);
private:
	int parse(const std::vector<std::string> &archs, const std::string &source,
		  antlr4::ANTLRInputStream &is, const EntryVisitor &entryVisitor);

	std::vector<std::string> m_archs;
	antlr4::ParserRuleContext *m_tree;
	std::unique_ptr<antlr4::ANTLRInputStream> m_input;
	std::unique_ptr<MakeLexer> m_lexer;
	std::unique_ptr<antlr4::CommonTokenStream> m_tokens;
	std::unique_ptr<MakeParser> m_parser;
};

}
