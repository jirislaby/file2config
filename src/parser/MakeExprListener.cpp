#include <iostream>
#include <string>

#include "MakeExprListener.h"

using namespace MP;

void MakeExprListener::exitExprAssign(MakeParser::ExprAssignContext *ctx)
{
	bool interesting = !ctx->l->getText().compare(0, lookingFor.length(), lookingFor);
	std::cerr << __func__ << ": lookingFor=" << lookingFor << " interesting=" << interesting
		  << ": " << ctx->getText().substr(0, 150) << "\n";
	std::cerr << "\tL='" << ctx->l->getText() << "' COND='" << ctx->l->cond << "'\n";
	for (const auto &a: ctx->l->children)
		std::cerr << "\t\t" << a->getText() << "\n";

	std::cerr << "\tOP='" << ctx->op->getText() << "'\n";

	std::string R("NUL");
	if (ctx->r)
		R = ctx->r->getText();
	std::cerr << "\tR='" << R.substr(0, 100) << "'\n";
	if (ctx->r && ctx->r->atom()) {
		for (const auto &atom: ctx->r->atom()->children) {
			const auto atomText = atom->getText();
			const auto atomTextLen = atomText.length();
			std::cerr << "\t\t" << atomText << "\n";
			if (!interesting)
				continue;
			if (atomText.back() == '/') {
				std::cerr << "\t\tDIRECTORY!\n";
				CB(ctx->l->cond, EntryType::Directory, atomText);
			} else if (atomTextLen > 2 && !atomText.compare(atomTextLen - 2, 2, ".o")) {
				std::cerr << "\t\tOBJECT!\n";
				CB(ctx->l->cond, EntryType::Object, atomText);
			}
		}
	}
}
