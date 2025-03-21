#include <iostream>
#include <errno.h>

#include "parser.h"
#include "MakeExprListener.h"

using namespace MP;

Parser::Parser()
{
}

int Parser::parse(const std::string &file, const MakeExprListener::Callback &CB)
{
	std::ifstream ifs;

	ifs.open(file);
	if (!ifs) {
		std::cerr << "cannot read " << file << ": " << strerror(errno) << "\n";
		return -1;
	}

	input = std::make_unique<antlr4::ANTLRInputStream>(ifs);
	lexer = std::make_unique<MakeLexer>(input.get());
	tokens = std::make_unique<antlr4::CommonTokenStream>(lexer.get());
	parser = std::make_unique<MakeParser>(tokens.get());
	// SLL is much faster, but may be incomplete
	auto interp = parser->getInterpreter<antlr4::atn::ParserATNSimulator>();
	interp->setPredictionMode(antlr4::atn::PredictionMode::SLL);
	tree = parser->makefile();
	if (parser->getNumberOfSyntaxErrors()) {
		std::cerr << "trying LL\n";
		tokens->reset();
		parser->reset();
		interp->setPredictionMode(antlr4::atn::PredictionMode::LL);
		tree = parser->makefile();
		if (parser->getNumberOfSyntaxErrors())
			return -1;
	}

	MakeExprListener l{ CB };
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

void Parser::findTarget(const std::string &target, const MakeExprListener::Callback &CB)
{
	antlr4::tree::ParseTreeWalker walker;
	MakeExprListener l{ CB, target + "-" };
	walker.walk(&l, tree);
}
