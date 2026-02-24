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
	const std::string &file;
};

}

#endif
