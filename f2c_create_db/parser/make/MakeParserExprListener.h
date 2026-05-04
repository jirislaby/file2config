// SPDX-License-Identifier: GPL-2.0-only

#ifndef MAKEEXPRLISTENER_H
#define MAKEEXPRLISTENER_H

#include <filesystem>
#include <string>
#include <string_view>

#include "MakeParserBaseListener.h"

namespace MP {

class EntryVisitor;

class MakeExprListener : public MakeParserBaseListener {
public:
	MakeExprListener() = delete;
	MakeExprListener(const std::vector<std::string> &archs, const EntryVisitor &entryVisitor,
			 const std::filesystem::path &rootDir, const std::filesystem::path &curDir)
		: MakeParserBaseListener(), archs(archs), entryVisitor(entryVisitor),
		m_rootDir(rootDir), m_curDir(curDir) {}

	virtual void exitExpr(MakeParser::ExprContext *) override;
	virtual void exitInclude(MakeParser::IncludeContext *ctx) override;

private:
	static bool isCompilerFlagsRule(std::string_view lhs);
	static MakeParser::IdContext *getEvalId(MakeParser::AtomContext *atom);

	std::vector<std::string> evaluateAtom(MakeParser::AtomContext *atom);
	std::vector<std::string> evaluateWord(MakeParser::WordContext *word);
	void evaluateWordAndVisit(const std::any &interesting, const std::string &lhs,
				  bool simpleAssign, const std::string &cond,
				  MakeParser::WordContext *word,
				  bool &resetVar);

	const std::vector<std::string> &archs;
	const EntryVisitor &entryVisitor;
	const std::filesystem::path &m_rootDir;
	const std::filesystem::path &m_curDir;
};

}

#endif
