// SPDX-License-Identifier: GPL-2.0-only

#ifndef MAKEEXPRLISTENER_H
#define MAKEEXPRLISTENER_H

#include <string>

#include "MakeBaseListener.h"

namespace MP {

class EntryVisitor;

class MakeExprListener : public MakeBaseListener {
public:
	MakeExprListener() = delete;
	MakeExprListener(const std::vector<std::string> &archs, const EntryVisitor &entryVisitor)
		: MakeBaseListener(), archs(archs), entryVisitor(entryVisitor) {}

	virtual void exitExpr(MakeParser::ExprContext *) override;
private:
	static bool isSubdirRule(const std::string &lhs);

	std::vector<std::string> evaluateAtom(MakeParser::AtomContext *atom);
	void evaluateWord(const std::any &interesting, const std::string &lhs,
			  const std::string &cond,
			  const MakeParser::WordContext *word);

	const std::vector<std::string> &archs;
	const EntryVisitor &entryVisitor;
};

}

#endif
