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
	auto parser = dynamic_cast<antlr4::Parser *>(recognizer);
	auto tokens = dynamic_cast<antlr4::CommonTokenStream *>(recognizer->getInputStream());
	Clr(std::cerr, Clr::RED) << "error: " << file << ":" << line << ":" << column << " " << msg;

	auto input = tokens->getTokenSource()->getInputStream()->toString();
	SlHelpers::GetLine gl(input);
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


	auto start = offendingSymbol->getStartIndex();
	auto stop = offendingSymbol->getStopIndex();
	Clr(std::cerr) << std::string(wsColumn, ' ') << std::string(stop - start + 1, '^');

	auto stack = parser->getRuleInvocationStack();
	std::cerr << "rule stack: [";
	SlHelpers::String::join(std::cerr, stack | std::views::reverse);
	std::cerr << "]\n";
}
