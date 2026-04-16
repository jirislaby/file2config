// SPDX-License-Identifier: GPL-2.0-only

#include <iostream>
#include <ranges>

#include <sl/helpers/Color.h>
#include <sl/helpers/String.h>

#include "ErrorListener.h"

using namespace Parsers;
using Clr = SlHelpers::Color;

void ErrorListener::syntaxError(antlr4::Recognizer *recognizer, antlr4::Token *offendingSymbol,
				size_t line, size_t column, const std::string &msg,
				std::exception_ptr)
{
	if (auto lexer = dynamic_cast<antlr4::Lexer *>(recognizer))
		lexerError(*lexer, line, column, msg);
	else if (auto parser = dynamic_cast<antlr4::Parser *>(recognizer))
		parserError(*parser, *offendingSymbol, line, column, msg);
}

void ErrorListener::dumpHeader(std::string_view type, size_t line, size_t column,
			       std::string_view msg) const
{
	Clr(std::cerr, Clr::RED) << type << " error: " << file << ':' << line << ':' << column <<
				    ' ' << msg;
}

size_t ErrorListener::dumpLine(antlr4::TokenSource &tokSrc, size_t line, size_t column) const
{
	auto inputStr = tokSrc.getInputStream()->toString();
	SlHelpers::GetLine gl(inputStr);
	decltype(gl.get()) errorLine;
	size_t l = 0;
	auto wsColumn = 0U;
	while ((errorLine = gl.get()))
		if (++l == line) {
			Clr(std::cerr) << *errorLine;
			for (auto i = 0U; i < column; ++i) {
				if ((*errorLine)[i] == '\t')
					wsColumn = (wsColumn + 8) & ~7;
				else
					wsColumn++;
			}
			break;
		}

	return wsColumn;
}

void ErrorListener::lexerError(antlr4::Lexer &lexer, size_t line, size_t column,
			       std::string_view msg) const
{
	dumpHeader("lexer", line, column, msg);
	auto wsColumn = dumpLine(lexer, line, column);
	Clr(std::cerr) << std::string(wsColumn, ' ') << '^';
}

void ErrorListener::parserError(antlr4::Parser &parser, antlr4::Token &offendingSymbol, size_t line,
				size_t column, std::string_view msg) const
{
	dumpHeader("parser", line, column, msg);
	Clr(std::cerr) << "token: " << offendingSymbol.toString();

	auto tokens = dynamic_cast<antlr4::CommonTokenStream *>(parser.getInputStream());
	auto wsColumn = dumpLine(*tokens->getTokenSource(), line, column);

	auto start = offendingSymbol.getStartIndex();
	auto stop = offendingSymbol.getStopIndex();
	Clr(std::cerr) << std::string(wsColumn, ' ') << std::string(stop - start + 1, '^');

	auto stack = parser.getRuleInvocationStack();
	std::cerr << "rule stack: [";
	SlHelpers::String::join(std::cerr, stack | std::views::reverse);
	std::cerr << "]\n";

	static auto constexpr const surround = 5U;
	std::cerr << "surrounding (at most) " << surround << " tokens: [";
	auto tokStart = offendingSymbol.getTokenIndex() > surround ?
				offendingSymbol.getTokenIndex() - surround : 0U;
	auto tokEnd = std::min(tokStart + surround * 2, tokens->size() - 1);
	SlHelpers::String::join(std::cerr, tokens->get(tokStart, tokEnd),
				[](auto &out, const auto &tok) {
		out << '[' << static_cast<ssize_t>(tok->getType()) << ",'";
		for (char c: tok->getText())
			switch (c) {
			case '\t':
				out << "\\t";
				break;
			case '\n':
				out << "\\n";
				break;
			default:
				out << c;
				break;
			}
		out << "']";
	});
	std::cerr << "]\n";
}