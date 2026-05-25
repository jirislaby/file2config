// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>

#include <sl/helpers/Color.h>

namespace F2C {

class StatusNotifier {
public:
	StatusNotifier() = delete;

	StatusNotifier(const std::string &branch, unsigned branchNo, unsigned branchCnt) :
		m_note(getNote(branch, branchNo, branchCnt)) { }

	void notify(std::string_view status) const {
		SlHelpers::Color(SlHelpers::Color::GREEN) << "== " << m_note << " -- " << status <<
			" ==";
	}
private:
	std::string getNote(const std::string &branch, unsigned branchNo,
			    unsigned branchCnt) const {
		auto percent = 100.0 * branchNo / branchCnt;
		std::ostringstream ss;
		ss << branch << " (" << branchNo << "/" << branchCnt << " -- " <<
			std::fixed << std::setprecision(2) << percent << " %)";
		return ss.str();
	}

	std::string m_note;
};

} // namespace
