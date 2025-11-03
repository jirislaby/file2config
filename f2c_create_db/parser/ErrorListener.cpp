// SPDX-License-Identifier: GPL-2.0-only

#include <iostream>

#include "ErrorListener.h"

using namespace MP;

void ErrorListener::syntaxError(antlr4::Recognizer *recognizer, antlr4::Token *offendingSymbol,
				size_t line, size_t column, const std::string &msg,
				std::exception_ptr)
{
	auto parser = dynamic_cast<antlr4::Parser *>(recognizer);
	auto tokens = dynamic_cast<antlr4::CommonTokenStream *>(recognizer->getInputStream());
	std::cerr << "error: " << file << ":" << line << ":" << column << " "
		  << msg << '\n';
	auto input = tokens->getTokenSource()->getInputStream()->toString();
	std::istringstream ISS(input);
	std::string errorLine;
	size_t l = 0;
	while (std::getline(ISS, errorLine, '\n')) {
		l++;
		if (l == line)
			break;
	}
	std::cerr << errorLine << '\n';
	for (unsigned i = 0; i < column; ++i)
		std::cerr << ' ';
	auto start = offendingSymbol->getStartIndex();
	auto stop = offendingSymbol->getStopIndex();
	for (unsigned i = 0; i < stop - start + 1; ++i)
		std::cerr << '^';
	std::cerr << '\n';
	auto stack = parser->getRuleInvocationStack();
	std::cerr << "rule stack: ";
	for (auto I = stack.rbegin(); I != stack.rend(); ++I)
		std::cerr << *I << ',';
	std::cerr << '\n';
}
