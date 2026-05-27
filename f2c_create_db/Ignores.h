// SPDX-License-Identifier: GPL-2.0-only

#include <filesystem>
#include <string>
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include "F2CSQLConn.h"

namespace F2C {

class Ignores {
private:
	using Json = nlohmann::ordered_json;
public:
	Ignores() = delete;

	static void process(F2CSQLConn &sql, const std::string &branch, const Json &json,
			    const std::filesystem::path &root);
private:
	static void processOne(F2CSQLConn &sql, const std::string &branch,
			       const std::vector<Json> &patterns,
			       const std::filesystem::path &relPath);
};

} // namespace
