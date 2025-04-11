// SPDX-License-Identifier: GPL-2.0-only

#include <iostream>
#include <errno.h>

#include "parser.h"
#include "ErrorListener.h"
#include "MakeExprListener.h"
#include "MakeLexer.h"
#include "MakeParser.h"

using namespace MP;

extern unsigned verbose;

Parser::Parser() {}

Parser::~Parser() {}

int Parser::parse(const std::vector<std::string> &archs, const std::string &file,
		  const EntryCallback *CB)
{
	std::ifstream ifs;

	this->archs = archs;
	ifs.open(file);
	if (!ifs) {
		std::cerr << "cannot read " << file << ": " << strerror(errno) << "\n";
		return -1;
	}

	input = std::make_unique<antlr4::ANTLRInputStream>(ifs);
	lexer = std::make_unique<MakeLexer>(input.get());
	tokens = std::make_unique<antlr4::CommonTokenStream>(lexer.get());
	parser = std::make_unique<MakeParser>(tokens.get());

	ErrorListener EL(file);
	parser->removeErrorListeners();
	if (verbose)
		parser->addErrorListener(&EL);

	// SLL is much faster, but may be incomplete
	auto interp = parser->getInterpreter<antlr4::atn::ParserATNSimulator>();
	interp->setPredictionMode(antlr4::atn::PredictionMode::SLL);
	tree = parser->makefile();
	if (parser->getNumberOfSyntaxErrors()) {
		std::cerr << file << ": SLL not enough, trying LL\n";
		if (!verbose)
			parser->addErrorListener(&EL);

		tokens->reset();
		parser->reset();
		interp->setPredictionMode(antlr4::atn::PredictionMode::LL);
		tree = parser->makefile();
		if (auto errs = parser->getNumberOfSyntaxErrors()) {
			std::cerr << file << ": LL failed to parse too: " << errs << " errors\n";
			return -1;
		}
	}

	MakeExprListener l{ archs, CB };
	antlr4::tree::ParseTreeWalker walker;
	walker.walk(&l, tree);

	return 0;
}

void Parser::reset()
{
	input.reset();
	lexer.reset();
	tokens.reset();
	parser.reset();
}

void Parser::walkTree(const EntryCallback *CB)
{
	antlr4::tree::ParseTreeWalker walker;
	MakeExprListener l{ archs, CB, };
	walker.walk(&l, tree);
}
