#include <iostream>
#include <string>

#include "MakeExprListener.h"

extern unsigned verbose;

using namespace MP;

std::vector<std::string> MakeExprListener::evaluateAtom(MakeParser::AtomContext *atom)
{
	std::vector<std::string> ret;

	if (auto e = atom->eval())
		if (auto ie = e->in_eval()) {
			if (ie->SRCARCH())
				return archs;
			if (ie->BITS())
				return { "32", "64" };
		}

	return { atom->getText() };
}

void MakeExprListener::evaluateWord(const std::string &cond, const MakeParser::WordContext *word)
{
	std::vector<std::string> evaluated;

	if (verbose > 1)
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
		const auto wordTextLen = wordText.length();
		if (verbose > 1)
			std::cout << "\t\t" << __func__ << ": " << wordText << "\n";

		if (wordText.back() == '/') {
			CB(cond, EntryType::Directory, wordText);
		} else if (wordTextLen > 2 && !wordText.compare(wordTextLen - 2, 2, ".o")) {
			CB(cond, EntryType::Object, wordText);
		}
	}
}

void MakeExprListener::exitExprAssign(MakeParser::ExprAssignContext *ctx)
{
	auto lText = ctx->l->getText();
	bool interesting = !lText.compare(0, lookingFor.length(), lookingFor);

	if (verbose > 1)
		std::cout << __func__ << ": lookingFor=" << lookingFor <<
			     " interesting=" << interesting << ": "
			  << ctx->getText().substr(0, 150) << "\n";

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
	}
	if (verbose > 1) {
		std::cout << "\tL='" << lText << "' COND='" << cond << "'\n";
		for (const auto &a: ctx->l->children)
			std::cout << "\t\t" << a->getText() << "\n";

		std::cout << "\tOP='" << ctx->op->getText() << "'\n";

		std::string R("NUL");
		if (ctx->r)
			R = ctx->r->getText();
		std::cout << "\tR='" << R.substr(0, 100) << "'\n";
	}

	if (!interesting)
		return;

	if (ctx->r && ctx->r->words()) {
		for (const auto &word: ctx->r->words()->w) {
			evaluateWord(cond, word);
		}
	}
}
