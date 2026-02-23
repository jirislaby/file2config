// SPDX-License-Identifier: GPL-2.0-only

#include <errno.h>

#include <sl/helpers/Color.h>
#include <sl/helpers/Misc.h>
#include <sl/helpers/String.h>

#include "ErrorListener.h"
#include "Parser.h"
#include "../Verbose.h"
#include "kconfig/KconfigLexer.h"
#include "kconfig/KconfigParser.h"
#include "make/MakeLexer.h"
#include "make/MakeParser.h"

using namespace Parsers;

using Clr = SlHelpers::Color;

template<class ALexer, class AParser>
Parser<ALexer, AParser>::Parser() {}

template<class ALexer, class AParser>
Parser<ALexer, AParser>::~Parser() {}

template<class ALexer, class AParser>
bool Parser<ALexer, AParser>::parseSLL(const std::string &source)
{
	auto origErrStrategy = m_parser->getErrorHandler();
	m_parser->setErrorHandler(std::make_shared<antlr4::BailErrorStrategy>());

	// SLL is much faster, but may be incomplete
	auto interp = m_parser->template getInterpreter<antlr4::atn::ParserATNSimulator>();
	interp->setPredictionMode(antlr4::atn::PredictionMode::SLL);
	try {
		m_tree = getTree();
		return true;
	} catch (antlr4::ParseCancellationException &) {
	}

	if (F2C::verbose)
		Clr(std::cerr, Clr::YELLOW) << source << ": SLL not enough, trying LL";

	interp->setPredictionMode(antlr4::atn::PredictionMode::LL);
	m_parser->setErrorHandler(origErrStrategy);

	m_tokens->reset();
	m_parser->reset();

	return parseLL(source);
}

template<class ALexer, class AParser>
bool Parser<ALexer, AParser>::parseLL(const std::string &source)
{
	m_lexer->removeErrorListeners();
	m_parser->removeErrorListeners();
	Parsers::ErrorListener EL(source);
	m_lexer->addErrorListener(&EL);
	m_parser->addErrorListener(&EL);

	try {
		m_tree = getTree();
		if (auto errs = m_parser->getNumberOfSyntaxErrors()) {
			Clr(std::cerr, Clr::RED) << source << ": LL failed to parse: " << errs << " errors";
			return false;
		}
	} catch (antlr4::ParseCancellationException &e) {
		Clr(std::cerr, Clr::RED) << source << ": LL failed to parse:\n" << e.what();
		return false;
	}
	return true;
}

template<class ALexer, class AParser>
bool Parser<ALexer, AParser>::parse(const std::string &source, antlr4::ANTLRInputStream &is,
				    bool trySLL)
{
	m_lexer = std::make_unique<ALexer>(&is);
	m_tokens = std::make_unique<antlr4::CommonTokenStream>(m_lexer.get());
	m_parser = std::make_unique<AParser>(m_tokens.get());

	if (trySLL)
		return parseSLL(source);

	return parseLL(source);
}

template<class ALexer, class AParser>
bool Parser<ALexer, AParser>::parse(std::string_view str, bool trySLL)
{
	m_input = std::make_unique<antlr4::ANTLRInputStream>(str);

	return parse("string", *m_input.get(), trySLL);
}

template<class ALexer, class AParser>
bool Parser<ALexer, AParser>::parse(const std::filesystem::path &file, bool trySLL)
{
	std::ifstream ifs;

	ifs.open(file);
	if (!ifs) {
		std::cerr << "cannot read " << file.string() << ": " << strerror(errno) << "\n";
		return false;
	}

	m_input = std::make_unique<antlr4::ANTLRInputStream>(ifs);

	return parse(file.string(), *m_input.get(), trySLL);
}

template<class ALexer, class AParser>
void Parser<ALexer, AParser>::reset()
{
	m_input.reset();
	m_lexer.reset();
	m_tokens.reset();
	m_parser.reset();
}

template class Parsers::Parser<KconfigLexer, KconfigParser>;
template class Parsers::Parser<MakeLexer, MakeParser>;
