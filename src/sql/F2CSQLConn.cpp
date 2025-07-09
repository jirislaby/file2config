// SPDX-License-Identifier: GPL-2.0-only

#include "F2CSQLConn.h"

using namespace SQL;

int F2CSQLConn::createDB()
{
	static const Tables create_tables {
		{ "branch", {
			"id INTEGER PRIMARY KEY",
			"branch TEXT NOT NULL UNIQUE",
			"sha TEXT NOT NULL"
		}},
		{ "config", {
			"id INTEGER PRIMARY KEY",
			"config TEXT NOT NULL UNIQUE"
		}},
		{ "dir", {
			"id INTEGER PRIMARY KEY",
			"dir TEXT NOT NULL UNIQUE"
		}},
		{ "file", {
			"id INTEGER PRIMARY KEY",
			"file TEXT NOT NULL",
			"dir INTEGER NOT NULL REFERENCES dir(id)",
			"UNIQUE(file, dir)"
		}},
		{ "conf_file_map", {
			"id INTEGER PRIMARY KEY",
			"branch INTEGER NOT NULL REFERENCES branch(id) ON DELETE CASCADE",
			"config INTEGER NOT NULL REFERENCES config(id) ON DELETE CASCADE",
			"file INTEGER NOT NULL REFERENCES file(id) ON DELETE CASCADE",
			"UNIQUE(branch, config, file)"
		}},
		{ "module", {
			"id INTEGER PRIMARY KEY",
			"dir INTEGER NOT NULL REFERENCES dir(id)",
			"module TEXT NOT NULL",
			"UNIQUE(dir, module)"
		}},
		{ "module_details_map", {
			"id INTEGER PRIMARY KEY",
			"branch INTEGER NOT NULL REFERENCES branch(id) ON DELETE CASCADE",
			"module INTEGER NOT NULL REFERENCES module(id) ON DELETE CASCADE",
			"supported INTEGER NOT NULL CHECK(supported >= -1 AND supported <= 4)",
			"UNIQUE(branch, module)"
		}},
		{ "module_file_map", {
			"id INTEGER PRIMARY KEY",
			"branch INTEGER NOT NULL REFERENCES branch(id) ON DELETE CASCADE",
			"module INTEGER NOT NULL REFERENCES module(id) ON DELETE CASCADE",
			"file INTEGER NOT NULL REFERENCES file(id) ON DELETE CASCADE",
			"UNIQUE(branch, module, file)"
		}},
		{ "user", {
			"id INTEGER PRIMARY KEY",
			"email TEXT NOT NULL UNIQUE"
		}},
		{ "user_file_map", {
			"id INTEGER PRIMARY KEY",
			"branch INTEGER NOT NULL REFERENCES branch(id) ON DELETE CASCADE",
			"user INTEGER NOT NULL REFERENCES user(id) ON DELETE CASCADE",
			"file INTEGER NOT NULL REFERENCES file(id) ON DELETE CASCADE",
			"count INTEGER NOT NULL",
			"count_no_fixes INTEGER NOT NULL",
			"UNIQUE(branch, user, file)"
		}},
	};

	static const Indices create_indexes {
		{ "conf_file_map_file_index", "conf_file_map(file)" },
	};

	static const Views create_views {
		{ "conf_file_map_view_raw_file",
			"SELECT map.id, branch.branch, config.config, map.file "
			"FROM conf_file_map AS map "
			"LEFT JOIN branch ON map.branch = branch.id "
			"LEFT JOIN config ON map.config = config.id;" },
		{ "conf_file_map_view",
			"SELECT map.id, map.branch, map.config, dir.dir || '/' || file.file AS path "
			"FROM conf_file_map_view_raw_file AS map "
			"LEFT JOIN file ON map.file = file.id "
			"LEFT JOIN dir ON file.dir = dir.id;" },
		{ "module_details_map_view",
			"SELECT map.id, branch.branch, "
				"module_dir.dir || '/' || module.module AS module, "
				"supported "
			"FROM module_details_map AS map "
			"LEFT JOIN module ON map.module = module.id "
			"LEFT JOIN dir AS module_dir ON module.dir = module_dir.id "
			"LEFT JOIN branch ON map.branch = branch.id;" },
		{ "module_file_map_view",
			"SELECT map.id, branch.branch, "
				"module_dir.dir || '/' || module.module AS module, "
				"dir.dir || '/' || file.file AS path "
			"FROM module_file_map AS map "
			"LEFT JOIN module ON map.module = module.id "
			"LEFT JOIN dir AS module_dir ON module.dir = module_dir.id "
			"LEFT JOIN branch ON map.branch = branch.id "
			"LEFT JOIN file ON map.file = file.id "
			"LEFT JOIN dir ON file.dir = dir.id;" },
		{ "user_file_map_view",
			"SELECT map.id, user.email, branch.branch, "
				"dir.dir || '/' || file.file AS path, "
				"map.count, map.count_no_fixes "
			"FROM user_file_map AS map "
			"LEFT JOIN user ON map.user = user.id "
			"LEFT JOIN branch ON map.branch = branch.id "
			"LEFT JOIN file ON map.file = file.id "
			"LEFT JOIN dir ON file.dir = dir.id;" },
		{ "user_file_map_view_grouped",
			"SELECT email, path, SUM(count) AS count, "
				"SUM(count_no_fixes) AS count_no_fixes "
			"FROM user_file_map_view GROUP BY email, path" },
	};

	if (createTables(create_tables) ||
			createIndices(create_indexes) ||
			createViews(create_views))
		return -1;

	return 0;
}

int F2CSQLConn::prepDB()
{
	if (prepareStatement("INSERT INTO branch(branch, sha)"
			     "VALUES (:branch, :sha);",
			     insBranch))
		return -1;

	if (prepareStatement("INSERT INTO config(config)"
			     "VALUES (:config);",
			     insConfig))
		return -1;

	if (prepareStatement("INSERT INTO dir(dir) VALUES (:dir);",
			     insDir))
		return -1;

	if (prepareStatement("INSERT INTO file(file, dir) "
			     "SELECT :file, dir.id FROM dir WHERE dir.dir = :dir;",
			     insFile))
		return -1;

	if (prepareStatement("INSERT INTO conf_file_map(branch, config, file) "
			     "SELECT branch.id, config.id, file.id FROM branch, config, file "
			     "LEFT JOIN dir ON file.dir = dir.id "
			     "WHERE branch.branch = :branch AND config.config = :config AND "
			     "file.file = :file AND dir.dir = :dir;",
			     insCFMap))
		return -1;

	if (prepareStatement("INSERT INTO module(dir, module) "
			     "SELECT dir.id, :module FROM dir WHERE dir.dir = :dir;",
			     insModule))
		return -1;

	if (prepareStatement("INSERT INTO module_details_map(branch, module, supported) "
			     "SELECT branch.id, module.id, :supported FROM branch, module "
			     "LEFT JOIN dir ON module.dir = dir.id "
			     "WHERE branch.branch = :branch AND dir.dir = :module_dir AND "
			     "module.module = :module;",
			     insMDMap))
		return -1;
	if (prepareStatement("INSERT INTO module_file_map(branch, module, file) "
			     "SELECT branch.id, module.id, file.id FROM branch, module, file "
			     "LEFT JOIN dir ON file.dir = dir.id "
			     "LEFT JOIN dir AS module_dir ON module.dir = module_dir.id "
			     "WHERE branch.branch = :branch AND module_dir.dir = :module_dir AND "
			     "module.module = :module AND "
			     "file.file = :file AND dir.dir = :dir;",
			     insMFMap))
		return -1;

	if (prepareStatement("INSERT INTO user(email) VALUES (:email);", insUser))
		return -1;

	if (prepareStatement("INSERT INTO user_file_map(user, branch, file, count, count_no_fixes) "
			     "SELECT user.id, branch.id, file.id, :count, :countnf "
				"FROM user, branch, file "
				"LEFT JOIN dir ON file.dir = dir.id "
				"WHERE user.email = :email AND branch.branch = :branch AND "
				"file.file = :file AND dir.dir = :dir;",
			     insUFMap))
		return -1;

	if (prepareStatement("DELETE FROM branch WHERE branch = :branch;", delBranch))
		return -1;

	if (prepareStatement("SELECT 1 FROM branch WHERE branch = :branch;", selBranch))
		return -1;

	return 0;
}

int F2CSQLConn::insertBranch(const std::string &branch, const std::string &sha)
{
	return insert(insBranch, {
			      { ":branch", branch },
			      { ":sha", sha },
		      });
}

int F2CSQLConn::insertConfig(const std::string &config)
{
	return insert(insConfig, { { ":config", config } });
}

int F2CSQLConn::insertDir(const std::string &dir)
{
	return insert(insDir, { { ":dir", dir } });
}

int F2CSQLConn::insertFile(const std::string &dir, const std::string &file)
{
	return insert(insFile, {
			      { ":dir", dir },
			      { ":file", file },
		      });
}

int F2CSQLConn::insertCFMap(const std::string &branch, const std::string &config,
			    const std::string &dir, const std::string &file)
{
	return insert(insCFMap, {
			      { ":branch", branch },
			      { ":config", config },
			      { ":dir", dir },
			      { ":file", file },
		      });
}

int F2CSQLConn::insertModule(const std::string &dir, const std::string &module)
{
	return insert(insModule, {
			      { ":dir", dir },
			      { ":module", module }
		      });
}

int F2CSQLConn::insertMDMap(const std::string &branch, const std::string &module_dir,
			    const std::string &module, int supported)
{
	return insert(insMDMap, {
			      { ":branch", branch },
			      { ":module_dir", module_dir },
			      { ":module", module },
			      { ":supported", supported },
		      });
}

int F2CSQLConn::insertMFMap(const std::string &branch, const std::string &module_dir,
			    const std::string &module, const std::string &dir, const std::string &file)
{
	return insert(insMFMap, {
			      { ":branch", branch },
			      { ":module_dir", module_dir },
			      { ":module", module },
			      { ":dir", dir },
			      { ":file", file },
		      });
}

int F2CSQLConn::insertUser(const std::string &email)
{
	return insert(insUser, { { ":email", email } });
}

int F2CSQLConn::insertUFMap(const std::string &branch, const std::string &email,
			    const std::string &dir, const std::string &file,
			    int count, int countnf)
{
	return insert(insUFMap, {
			      { ":branch", branch },
			      { ":email", email },
			      { ":dir", dir },
			      { ":file", file },
			      { ":count", count },
			      { ":countnf", countnf },
		      });
}

int F2CSQLConn::deleteBranch(const std::string &branch)
{
	return insert(delBranch, { { ":branch", branch } });
}

std::optional<bool> F2CSQLConn::hasBranch(const std::string &branch)
{
	SlSqlite::SQLConn::SelectResult res;

	if (select(selBranch, { { ":branch", branch } }, { typeid(int) }, res))
		return {};

	return res.size() && std::get<int>(res[0][0]) == 1;
}
