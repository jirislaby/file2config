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

int Parser::parse(const std::vector<std::string> &archs, const std::string &source,
		  antlr4::ANTLRInputStream &is, const EntryVisitor &entryVisitor)
{
	m_lexer = std::make_unique<MakeLexer>(&is);
	m_tokens = std::make_unique<antlr4::CommonTokenStream>(m_lexer.get());
	m_parser = std::make_unique<MakeParser>(m_tokens.get());

	auto origErrStrategy = m_parser->getErrorHandler();
	m_parser->setErrorHandler(std::make_shared<antlr4::BailErrorStrategy>());

	// SLL is much faster, but may be incomplete
	auto interp = m_parser->getInterpreter<antlr4::atn::ParserATNSimulator>();
	interp->setPredictionMode(antlr4::atn::PredictionMode::SLL);
	try {
		m_tree = m_parser->makefile();
	} catch (antlr4::ParseCancellationException &) {
		if (F2C::verbose)
			std::cerr << source << ": SLL not enough, trying LL\n";

		m_parser->removeErrorListeners();
		ErrorListener EL(source);
		m_parser->addErrorListener(&EL);

		m_parser->setErrorHandler(origErrStrategy);

		m_tokens->reset();
		m_parser->reset();
		interp->setPredictionMode(antlr4::atn::PredictionMode::LL);
		m_tree = m_parser->makefile();
		if (auto errs = m_parser->getNumberOfSyntaxErrors()) {
			std::cerr << source << ": LL failed to parse too: " << errs << " errors\n";
			return -1;
		}
	}

	MakeExprListener l{ archs, entryVisitor };
	antlr4::tree::ParseTreeWalker walker;
	walker.walk(&l, m_tree);

	return 0;
}

int Parser::parse(const std::vector<std::string> &archs, const std::string &str,
		  const EntryVisitor &entryVisitor)
{
	m_input = std::make_unique<antlr4::ANTLRInputStream>(str);

	return parse(archs, "string", *m_input.get(), entryVisitor);
}

int Parser::parse(const std::vector<std::string> &archs, const std::filesystem::path &file,
		  const EntryVisitor &entryVisitor)
{
	std::ifstream ifs;

	this->m_archs = archs;
	ifs.open(file);
	if (!ifs) {
		std::cerr << "cannot read " << file.string() << ": " << strerror(errno) << "\n";
		return -1;
	}

	m_input = std::make_unique<antlr4::ANTLRInputStream>(ifs);

	return parse(archs, file.string(), *m_input.get(), entryVisitor);
}

void Parser::reset()
{
	m_input.reset();
	m_lexer.reset();
	m_tokens.reset();
	m_parser.reset();
}

void Parser::walkTree(const EntryVisitor &entryVisitor)
{
	antlr4::tree::ParseTreeWalker walker;
	MakeExprListener l{ m_archs, entryVisitor };
	walker.walk(&l, m_tree);
}
