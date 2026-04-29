// SPDX-License-Identifier: GPL-2.0-only

#include <filesystem>
#include <iostream>
#include <string>

#include <sl/helpers/Color.h>
#include <sl/helpers/String.h>

#include "EntryVisitor.h"
#include "MakeLexer.h"
#include "MakeParserExprListener.h"
#include "../../Verbose.h"

using namespace MP;
using Clr = SlHelpers::Color;

MakeParser::IdContext *MakeExprListener::getEvalId(MakeParser::AtomContext *atom)
{
	if (auto e = atom->eval())
		if (auto ie = e->in_eval())
			if (auto a = ie->a1)
				if (auto id = a->id())
					return id;

	return nullptr;
}

std::vector<std::string> MakeExprListener::evaluateAtom(MakeParser::AtomContext *atom)
{
	if (auto id = getEvalId(atom)) {
		if (id->CSKYABI())
			return { "abiv1", "abiv2" };
		if (id->SRCARCH())
			return archs;
		if (id->BITS())
			return { "32", "64" };
		auto text = id->getText();
		if (text == "src")
			return { m_curDir };
		if (text == "srctree")
			return { m_rootDir };

		if (auto res = entryVisitor.getVariable(text); !res.empty())
			return res;
	}

	return { atom->getText() };
}

std::vector<std::string> MakeExprListener::evaluateWord(MakeParser::WordContext *word)
{
	std::vector<std::string> evaluated;

	for (const auto &atom: word->children) {
		std::vector<std::string> newRes;

		auto evalAtom = evaluateAtom(dynamic_cast<MakeParser::AtomContext *>(atom));

		if (evaluated.empty()) {
			evaluated = evalAtom;
		} else {
			for (const auto &entry: evalAtom)
				for (const auto &evaluatedEntry: evaluated)
					newRes.push_back(evaluatedEntry + entry);

			evaluated = newRes;
		}
	}

	if (F2C::verbose > 1) {
		Clr() << __func__ << ": " << word->getText() << " -> [" << Clr::NoNL;
		SlHelpers::String::join(std::cout, evaluated);
		Clr()<< ']';
	}

	return evaluated;
}

bool MakeExprListener::isCompilerFlagsRule(std::string_view lhs)
{
	return lhs.starts_with("subdir-asflags-") || lhs.starts_with("subdir-ccflags-");
}

void MakeExprListener::evaluateWordAndVisit(const std::any &interesting, const std::string &lhs,
					    bool simpleAssign, const std::string &cond,
					    MakeParser::WordContext *word,
					    bool &resetVar)
{
	for (auto &wordText: evaluateWord(word)) {
		if (F2C::verbose > 2)
			std::cout << "\t\t" << __func__ << ": lhs=" << lhs << " rhs=" << wordText
				<< "\n";

		if (simpleAssign)
			entryVisitor.setVariable(lhs, resetVar, wordText);

		resetVar = false;

		if (!interesting.has_value())
			continue;

		if (!isCompilerFlagsRule(lhs) &&
		    (wordText.back() == '/' || lhs.starts_with("subdir-"))) {
			entryVisitor.entry(interesting, cond, EntryType::Directory,
					   std::move(wordText));
		} else if (wordText.ends_with(".o")) {
			entryVisitor.entry(interesting, cond, EntryType::Object,
					   std::move(wordText));
		}
	}
}

void MakeExprListener::exitExpr(MakeParser::ExprContext *ctx)
{
	auto lText = ctx->l->getText();
	auto interesting = entryVisitor.isInteresting(lText);

	if (F2C::verbose > 2) {
		std::cout << __func__ << ": interesting=" << interesting.has_value() << ": "
			  << ctx->getText().substr(0, 150) << '\n';
	}

	/*
	 * either it came as obj-$(CONFIG_) or obj-y and is set already or it is some target-y
	 * and we need to compute
	 */
	auto cond = ctx->l->cond;
	if (cond.empty()) {
		static constexpr std::string_view suffixes[] = { "-y", "-m", "-objs" };

		for (const auto &s: suffixes)
			if (lText.ends_with(s)) {
				cond = s.substr(1);
				break;
			}
	}
	if (F2C::verbose > 2) {
		std::cout << "\tL='" << lText << "' COND='" << cond << "'\n";
		for (const auto &a: ctx->l->children)
			std::cout << "\t\t" << a->getText() << "\n";

		std::cout << "\tOP='" << ctx->op->getText() << "'\n";

		std::string R("NUL");
		if (ctx->r)
			R = ctx->r->getText();
		std::cout << "\tR='" << R.substr(0, 100) << "'\n";
	}

	if (ctx->r && ctx->r->words()) {
		auto opType = ctx->op->getType();
		auto resetVar = opType == MakeLexer::EQ || opType == MakeLexer::ASSIGN;
		auto simpleAssign = ctx->l->children.size() == 1;

		for (const auto &word: ctx->r->words()->w)
			evaluateWordAndVisit(interesting, lText, simpleAssign, cond, word,
					     resetVar);
	}
}

void MakeExprListener::exitInclude(MakeParser::IncludeContext *ctx)
{
	auto inc = ctx->word();
	for (const auto &e: evaluateWord(inc)) {
		std::filesystem::path dest { e };
		if (F2C::verbose > 1)
			Clr(std::cerr) << __func__ << ": include: " << inc->getText() <<
				" -> " << dest;
		if (std::filesystem::exists(dest)) {
			entryVisitor.include(std::move(dest));
			continue;
		}
		// pre-6.3 trees used --include-dir=$(abs_srctree)
		auto destIncludeDir = m_rootDir / dest;
		if (std::filesystem::exists(destIncludeDir)) {
			entryVisitor.include(std::move(destIncludeDir));
			continue;
		}

		if (F2C::verbose > 0)
			Clr(std::cerr, Clr::YELLOW) << "include " << dest <<
				" does not exist, cwd=" << m_curDir;
	}
}

std::string MakeExprListener::handleIfeq(MakeParser::Ifeq_condContext *ieCond, bool neq)
{
	if (!ieCond->LPAREN())
		return "y";

	const auto expectedRSize = neq ? 0U : 1U;
	if (ieCond->l.size() != 1 || ieCond->r.size() != expectedRSize)
		return "y";

	if (auto lAtom = ieCond->l[0]->atom())
		if (auto id = getEvalId(lAtom))
			if (id->CONFIG()) {
				// r was checked above to be empty, so we handle only:
				// ifneq ($(CONFIG_FOO),)
				if (neq)
					return id->getText();

				// ifeq ($(CONFIG_FOO),X) where X is y or m
				if (auto rAtom = ieCond->r[0]->atom()) {
					auto rAtomText = rAtom->getText();
					if (rAtomText == "y" || rAtomText == "m")
						return id->getText();
				}
			}

	return "y";
}

void MakeExprListener::exitIfeq_exprCommon(MakeParser::Ifeq_exprContext *ctx)
{
	std::string cond{"y"};

	if (ctx) {
		if (ctx->IFNDEF()) {
			// we have no way to note negatives in the DB
		} else if (ctx->IFDEF()) {
			if (auto conf = ctx->CONFIG())
				cond = conf->getText();
		} else if (ctx->IFNEQ()) {
			cond = handleIfeq(ctx->ifeq_cond(), true);
		} else if (ctx->IFEQ()) {
			cond = handleIfeq(ctx->ifeq_cond(), false);
		}
		if (F2C::verbose > 2) {
			auto start = ctx->getStart();
			auto src = start->getInputStream()->getSourceName();
			Clr(std::cerr) << __func__ << ": " << src << ':' << start->getLine() <<
				": " << ctx->getText() << " -> " << cond;
		}
	}

	entryVisitor.enterConditional(std::move(cond));
}

void MakeExprListener::exitConditional_ifeq_expr(MakeParser::Conditional_ifeq_exprContext *ctx)
{
	exitIfeq_exprCommon(ctx->ifeq_expr());
}

void MakeExprListener::exitConditional_ifeq_expr_cond(MakeParser::Conditional_ifeq_expr_condContext *ctx)
{
	exitIfeq_exprCommon(ctx->ifeq_expr());
}

void MakeExprListener::exitConditional_body(MakeParser::Conditional_bodyContext *ctx)
{
	entryVisitor.exitConditional();
}
