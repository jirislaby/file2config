// SPDX-License-Identifier: GPL-2.0-only

#ifndef MAKEEXPRLISTENER_H
#define MAKEEXPRLISTENER_H

#include <string>

#include "MakeParserBaseListener.h"

namespace MP {

class EntryVisitor;

class MakeExprListener : public MakeParserBaseListener {
public:
	MakeExprListener() = delete;
	MakeExprListener(const std::vector<std::string> &archs, const EntryVisitor &entryVisitor)
		: MakeParserBaseListener(), archs(archs), entryVisitor(entryVisitor) {}

	virtual void exitExpr(MakeParser::ExprContext *) override;
private:
	static bool isSubdirRule(const std::string &lhs);

	std::vector<std::string> evaluateAtom(MakeParser::AtomContext *atom);
	std::vector<std::string> evaluateWord(const MakeParser::WordContext *word);
	void evaluateWordAndVisit(const std::any &interesting, const std::string &lhs,
				  const std::string &cond,
				  const MakeParser::WordContext *word);

	const std::vector<std::string> &archs;
	const EntryVisitor &entryVisitor;
};

}

#endif
