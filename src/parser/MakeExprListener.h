// SPDX-License-Identifier: GPL-2.0-only

#ifndef MAKEEXPRLISTENER_H
#define MAKEEXPRLISTENER_H

#include <string>

#include "MakeBaseListener.h"

namespace MP {

class EntryCallback {
public:
	enum EntryType {
		Directory,
		Object,
	};

	virtual const std::any isInteresting(const std::string &lhs) const = 0;

	virtual void entry(const std::any &interesting, const std::string &cond,
			   const enum EntryType &type, const std::string &word) const = 0;
};

class MakeExprListener : public MakeBaseListener {
public:
	MakeExprListener() = delete;
	MakeExprListener(const std::vector<std::string> &archs, const EntryCallback *EC)
		: MakeBaseListener(), archs(archs), EC(EC) {}

	virtual void exitExpr(MakeParser::ExprContext *) override;
private:
	static bool isSubdirRule(const std::string &lhs);

	std::vector<std::string> evaluateAtom(MakeParser::AtomContext *atom);
	void evaluateWord(const std::any &interesting, const std::string &lhs,
			  const std::string &cond,
			  const MakeParser::WordContext *word);

	const std::vector<std::string> &archs;
	const EntryCallback *EC;
};

}

#endif
