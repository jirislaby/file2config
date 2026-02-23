// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <functional>

#include "../Parser.h"
#include "Config.h"

class KconfigLexer;
class KconfigParser;

namespace Kconfig {

class Parser : public Parsers::Parser<KconfigLexer, KconfigParser> {
public:
	using ConfigCB = std::function<void (std::string config, ConfType type)>;

	void walkConfigs(ConfigCB configCB) const;
protected:
	virtual antlr4::ParserRuleContext *getTree();
};

}
