// SPDX-License-Identifier: GPL-2.0-only

#ifndef ERRORLISTENER_H
#define ERRORLISTENER_H

#include <antlr4-runtime.h>

namespace Parsers {

class ErrorListener : public antlr4::BaseErrorListener {
public:
	ErrorListener() = delete;
	ErrorListener(const std::string &file) : file(file) {}

	virtual void syntaxError(antlr4::Recognizer *recognizer, antlr4::Token *offendingSymbol,
				 size_t line, size_t column, const std::string &msg,
				 std::exception_ptr e) override;

private:
	void dumpHeader(std::string_view type, size_t line, size_t column,
			std::string_view msg) const;
	size_t dumpLine(antlr4::TokenSource &tokSrc, size_t line, size_t column) const;
	void lexerError(antlr4::Lexer &lexer, size_t line, size_t column,
			std::string_view msg) const;
	void parserError(antlr4::Parser &parser, antlr4::Token &offendingSymbol,
			 size_t line, size_t column, std::string_view msg) const;

	const std::string &file;
};

}

#endif
