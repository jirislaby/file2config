// SPDX-License-Identifier: GPL-2.0-only

#include <cxxopts.hpp>

#include <sl/helpers/Color.h>

#include "Opts.h"
#include "Verbose.h"

using Clr = SlHelpers::Color;
using namespace F2C;

Opts Opts::getOpts(int argc, char **argv)
{
	cxxopts::Options options { argv[0], "Generate conf_file_map database (and more)" };
	Opts opts;
	options.add_options()
		("a,append-branch", "process also this branch", cxxopts::value(opts.appendBranches))
		("b,branch", "branch to process", cxxopts::value(opts.branches))
		("force-color", "force color output")
		("dest", "destination (scratch area)",
			cxxopts::value(opts.dest)->default_value("$SCRATCH_AREA/fill-db"))
		("f,force", "force branch creation (delete old data)",
			cxxopts::value(opts.force)->default_value("false"))
		("no-fetch", "work offline, no updates of repos",
			cxxopts::value(opts.noFetch)->default_value("false"))
		("no-renames", "do not detect and store file renames",
			cxxopts::value(opts.noRenames)->default_value("false"))
		("q,quiet", "quiet mode", cxxopts::value(F2C::quiet)->default_value("false"))
		("v,verbose", "verbose mode")
	;
	options.add_options("authors")
		("authors-dump-refs", "dump references to stdout",
			cxxopts::value(opts.authorsDumpRefs)->default_value("false"))
		("authors-report-unhandled", "report unhandled lines to stdout",
			cxxopts::value(opts.authorsReportUnhandled)->default_value("false"))
		("authors-valid-users", "list of valid users (insert only these into the db)",
			cxxopts::value(opts.authorsValidUsers))
		("authors-LDAP-password-file", "file containting the password to the SUSE LDAP",
			cxxopts::value(opts.authorsLDAPPasswordFile));
	;
	options.add_options("files")
		("configuration", "path to JSON containing configuration to be used",
			cxxopts::value(opts.configurationJSON))
	;
	options.add_options("sqlite")
		("s,sqlite", "db name",
			cxxopts::value(opts.sqlite)->default_value("conf_file_map.sqlite"))
		("S,sqlite-create", "create the db if not exists",
			cxxopts::value(opts.sqliteCreate)->default_value("false"))
		("O,sqlite-create-only", "only create the db (do not fill it)",
			cxxopts::value(opts.sqliteCreateOnly)->default_value("false"))
	;

	try {
		auto cxxopts = options.parse(argc, argv);
		F2C::verbose = cxxopts.count("verbose");
		Clr::forceColor(cxxopts.contains("force-color"));
		Clr::forceColorValue(cxxopts.contains("force-color"));
		opts.hasDest = cxxopts.contains("dest");
		opts.hasConfiguration = cxxopts.contains("configuration");
		return opts;
	} catch (const cxxopts::exceptions::parsing &e) {
		Clr(std::cerr, Clr::RED) << "arguments error: " << e.what();
		std::cerr << options.help();
		exit(EXIT_FAILURE);
	}
}
