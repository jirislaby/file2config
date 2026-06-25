// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace F2C {

struct Opts {
	std::vector<std::string> appendBranches;
	std::vector<std::string> branches;
	std::filesystem::path dest;
	bool hasDest;
	bool force;
	bool noFetch;
	bool noRenames;
	bool quiet;
	unsigned verbose;

	bool authorsDumpRefs;
	bool authorsReportUnhandled;
	std::filesystem::path authorsValidUsers;
	std::filesystem::path authorsLDAPPasswordFile;

	std::filesystem::path configurationJSON;
	bool hasConfiguration;

	std::filesystem::path sqlite;
	bool sqliteCreate;
	bool sqliteCreateOnly;

	static Opts getOpts(int argc, char **argv);
};

} // namespace
