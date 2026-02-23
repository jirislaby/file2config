// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include "KconfigParserBaseListener.h"
#include "Parser.h"

namespace Kconfig {

class KconfigParserConfigListener : public KconfigParserBaseListener {
public:
	KconfigParserConfigListener() = delete;
	KconfigParserConfigListener(Parser::ConfigCB configCB) :
		KconfigParserBaseListener(), m_configCB(std::move(configCB)) {}

	virtual void exitConfig(KconfigParser::ConfigContext *ctx) override;
private:
	Parser::ConfigCB m_configCB;
};

}
