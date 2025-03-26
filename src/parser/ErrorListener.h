#ifndef ERRORLISTENER_H
#define ERRORLISTENER_H

#include <antlr4-runtime.h>

namespace MP {

class ErrorListener : public antlr4::BaseErrorListener {
public:
    virtual void syntaxError(antlr4::Recognizer *recognizer, antlr4::Token *offendingSymbol,
                             size_t line, size_t column, const std::string &msg,
                             std::exception_ptr e) override;

    static ErrorListener INSTANCE;
};

}

#endif
