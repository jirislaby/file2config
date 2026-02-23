// SPDX-License-Identifier: GPL-2.0-only

#include "KconfigLexer.h"
#include "KconfigParserConfigListener.h"

using namespace Kconfig;

void KconfigParserConfigListener::exitConfig(KconfigParser::ConfigContext *ctx)
{
	static const constinit struct {
		ConfType ctype;
		unsigned ptype;
	} conv[] = {
		{ ConfType::Bool, KconfigLexer::Bool },
		{ ConfType::Tristate, KconfigLexer::Tristate },
		{ ConfType::DefBool, KconfigLexer::Def_bool },
		{ ConfType::DefTristate, KconfigLexer::Def_tristate },
		{ ConfType::Int, KconfigLexer::Int },
		{ ConfType::Hex, KconfigLexer::Hex },
		{ ConfType::String, KconfigLexer::String },
		{ ConfType::Range, KconfigLexer::Range },
	};

	/*
	 * Some configs are declared somewhere and its value defined elsewhere.
	 * Like ARCH_MMAP_RND_BITS_MIN.
	 */
	for (const auto &l: ctx->config_line())
		if (l->type)
			for (const auto &c: conv)
				if (l->type->getType() == c.ptype) {
					m_configCB(ctx->config_id()->getText(), c.ctype);
					return;
				}
}
