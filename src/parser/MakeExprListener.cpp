#include <iostream>
#include <string>

#include "MakeExprListener.h"

using namespace MP;

void MakeExprListener::exitExprAssign(MakeParser::ExprAssignContext *ctx)
{
	auto lText = ctx->l->getText();
	bool interesting = !lText.compare(0, lookingFor.length(), lookingFor);

	std::cerr << __func__ << ": lookingFor=" << lookingFor << " interesting=" << interesting
		  << ": " << ctx->getText().substr(0, 150) << "\n";
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
	std::cerr << "\tL='" << lText << "' COND='" << cond << "'\n";
	for (const auto &a: ctx->l->children)
		std::cerr << "\t\t" << a->getText() << "\n";

	std::cerr << "\tOP='" << ctx->op->getText() << "'\n";

	std::string R("NUL");
	if (ctx->r)
		R = ctx->r->getText();
	std::cerr << "\tR='" << R.substr(0, 100) << "'\n";
	if (ctx->r && ctx->r->atoms()) {
		for (const auto &atom: ctx->r->atoms()->children) {
			const auto atomText = atom->getText();
			const auto atomTextLen = atomText.length();
			std::cerr << "\t\t" << atomText << "\n";
			if (!interesting)
				continue;
			if (atomText.back() == '/') {
				CB(cond, EntryType::Directory, atomText);
			} else if (atomTextLen > 2 && !atomText.compare(atomTextLen - 2, 2, ".o")) {
				CB(cond, EntryType::Object, atomText);
			}
		}
	}
}
