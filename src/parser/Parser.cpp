// SPDX-License-Identifier: GPL-2.0-only

#include <iostream>
#include <errno.h>

#include "ErrorListener.h"
#include "MakeExprListener.h"
#include "MakeLexer.h"
#include "MakeParser.h"
#include "Parser.h"
#include "../Verbose.h"

using namespace MP;

Parser::Parser() {}

Parser::~Parser() {}

int Parser::parse(const std::vector<std::string> &archs, const std::filesystem::path &file,
		  const EntryVisitor &entryVisitor)
{
	std::ifstream ifs;

	this->archs = archs;
	ifs.open(file);
	if (!ifs) {
		std::cerr << "cannot read " << file.string() << ": " << strerror(errno) << "\n";
		return -1;
	}

	input = std::make_unique<antlr4::ANTLRInputStream>(ifs);
	lexer = std::make_unique<MakeLexer>(input.get());
	tokens = std::make_unique<antlr4::CommonTokenStream>(lexer.get());
	parser = std::make_unique<MakeParser>(tokens.get());

	auto origErrStrategy = parser->getErrorHandler();
	parser->setErrorHandler(std::make_shared<antlr4::BailErrorStrategy>());

	// SLL is much faster, but may be incomplete
	auto interp = parser->getInterpreter<antlr4::atn::ParserATNSimulator>();
	interp->setPredictionMode(antlr4::atn::PredictionMode::SLL);
	try {
		tree = parser->makefile();
	} catch (antlr4::ParseCancellationException &) {
		if (F2C::verbose)
			std::cerr << file.string() << ": SLL not enough, trying LL\n";

		parser->removeErrorListeners();
		ErrorListener EL(file.string());
		parser->addErrorListener(&EL);

		parser->setErrorHandler(origErrStrategy);

		tokens->reset();
		parser->reset();
		interp->setPredictionMode(antlr4::atn::PredictionMode::LL);
		tree = parser->makefile();
		if (auto errs = parser->getNumberOfSyntaxErrors()) {
			std::cerr << file.string() << ": LL failed to parse too: " <<
				     errs << " errors\n";
			return -1;
		}
	}

	MakeExprListener l{ archs, entryVisitor };
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

void Parser::walkTree(const EntryVisitor &entryVisitor)
{
	antlr4::tree::ParseTreeWalker walker;
	MakeExprListener l{ archs, entryVisitor };
	walker.walk(&l, tree);
}
