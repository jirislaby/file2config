// SPDX-License-Identifier: GPL-2.0-only

#include <iostream>
#include <string>

#include "EntryCallback.h"
#include "MakeExprListener.h"
#include "../Verbose.h"

using namespace MP;

bool MakeExprListener::isSubdirRule(const std::string &lhs)
{
	static const std::string subdir { "subdir-" };
	static const std::string subdirAsFlags { "asflags-" };
	static const std::string subdirCCFlags { "ccflags-" };

	if (lhs.compare(0, subdir.length(), subdir))
		return false;

	if (!lhs.compare(subdir.length(), subdirAsFlags.length(), subdirAsFlags))
		return false;

	if (!lhs.compare(subdir.length(), subdirCCFlags.length(), subdirCCFlags))
		return false;

	return true;
}

std::vector<std::string> MakeExprListener::evaluateAtom(MakeParser::AtomContext *atom)
{
	if (auto e = atom->eval())
		if (auto ie = e->in_eval())
			if (auto a = ie->a1)
				if (auto id = a->id()) {
					if (id->CSKYABI())
						return { "abiv1", "abiv2" };
					if (id->SRCARCH())
						return archs;
					if (id->BITS())
						return { "32", "64" };
				}

	return { atom->getText() };
}

void MakeExprListener::evaluateWord(const std::any &interesting, const std::string &lhs,
				    const std::string &cond,
				    const MakeParser::WordContext *word)
{
	std::vector<std::string> evaluated;

	if (F2C::verbose > 1)
		std::cout << __func__ << ": " << const_cast<MakeParser::WordContext *>(word)->getText() << '\n';

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

	for (const auto &wordText: evaluated) {
		if (F2C::verbose > 2)
			std::cout << "\t\t" << __func__ << ": " << wordText << "\n";

		const auto wordTextLen = wordText.length();
		if (wordText.back() == '/' || isSubdirRule(lhs)) {
			EC.entry(interesting, cond, EntryType::Directory, wordText);
		} else if (wordTextLen > 2 && !wordText.compare(wordTextLen - 2, 2, ".o")) {
			EC.entry(interesting, cond, EntryType::Object, wordText);
		}
	}
}

void MakeExprListener::exitExpr(MakeParser::ExprContext *ctx)
{
	auto lText = ctx->l->getText();
	std::any interesting = EC.isInteresting(lText);

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
		auto lTextLen = lText.length();
		for (const auto &s: { "-y", "-m", "-objs" }) {
			auto sLen = strlen(s);
			if (lTextLen > sLen && !lText.compare(lTextLen - sLen, sLen, s)) {
				cond = s + 1;
				break;
			}
		}
		if (0 && !cond.empty())
			std::cout << __func__ << ": empty cond: " << ctx->getText() << '\n';
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

	if (!interesting.has_value())
		return;

	if (ctx->r && ctx->r->words()) {
		for (const auto &word: ctx->r->words()->w) {
			evaluateWord(interesting, lText, cond, word);
		}
	}
}
