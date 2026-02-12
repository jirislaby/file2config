// SPDX-License-Identifier: GPL-2.0-only

#include <sl/helpers/Exception.h>

#include "F2CSQLConn.h"

using namespace SQL;
using RunEx = SlHelpers::RuntimeException;
using SlHelpers::raise;

bool F2CSQLConn::createDB()
{
	static const Tables create_tables {
		{ "branch", {
			"id INTEGER PRIMARY KEY",
			"branch TEXT NOT NULL UNIQUE",
			"sha TEXT NOT NULL",
			"version INTEGER NOT NULL",
		}},
		{ "config", {
			"id INTEGER PRIMARY KEY",
			"config TEXT NOT NULL UNIQUE"
		}},
		{ "arch", {
			"id INTEGER PRIMARY KEY",
			"arch TEXT NOT NULL UNIQUE"
		}},
		{ "flavor", {
			"id INTEGER PRIMARY KEY",
			"flavor TEXT NOT NULL UNIQUE"
		}},
		{ "conf_branch_map", {
			"id INTEGER PRIMARY KEY",
			"branch INTEGER NOT NULL REFERENCES branch(id) ON DELETE CASCADE",
			"arch INTEGER NOT NULL REFERENCES arch(id) ON DELETE CASCADE",
			"flavor INTEGER NOT NULL REFERENCES flavor(id) ON DELETE CASCADE",
			"config INTEGER NOT NULL REFERENCES config(id) ON DELETE CASCADE",
			"value TEXT NOT NULL CHECK(value IN ('n', 'y', 'm') OR "
				"substr(value, 1, 1) = 'v')",
			"UNIQUE(branch, config, arch, flavor)"
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
		{ "conf_dep", {
			"branch INTEGER NOT NULL REFERENCES branch(id) ON DELETE CASCADE",
			"parent INTEGER NOT NULL REFERENCES config(id) ON DELETE CASCADE",
			"child  INTEGER NOT NULL REFERENCES config(id) ON DELETE CASCADE",
			"PRIMARY KEY(branch, parent, child)",
			"CHECK(parent != child)"
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
			"supported INTEGER NOT NULL CHECK(supported >= -3 AND supported <= 4)",
			"UNIQUE(branch, module)"
		}},
		{ "module_file_map", {
			"branch INTEGER NOT NULL REFERENCES branch(id) ON DELETE CASCADE",
			"module INTEGER NOT NULL REFERENCES module(id) ON DELETE CASCADE",
			"file INTEGER NOT NULL REFERENCES file(id) ON DELETE CASCADE",
			"PRIMARY KEY(branch, module, file)"
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
		{ "ignored_file_branch_map", {
			"branch INTEGER NOT NULL REFERENCES branch(id) ON DELETE CASCADE",
			"file INTEGER NOT NULL REFERENCES file(id) ON DELETE CASCADE",
			"PRIMARY KEY(branch, file)"
		}},
		{ "rename_file_version_map", {
			"version INTEGER NOT NULL CHECK(version > 0)",
			"similarity INTEGER NOT NULL CHECK(similarity BETWEEN 0 AND 100)",
			"oldfile INTEGER NOT NULL REFERENCES file(id) ON DELETE CASCADE",
			"newfile INTEGER NOT NULL REFERENCES file(id) ON DELETE CASCADE",
			"PRIMARY KEY (version, oldfile, newfile)",
			"UNIQUE(version, oldfile)",
			"UNIQUE(version, newfile)"
		}},
	};

	static const Indices create_indexes {
		{ "conf_file_map_file_index", "conf_file_map(file)" },
		{ "conf_dep_branch_child_index", "conf_dep(branch, child)" },
	};

	static const Views create_views {
		{ "conf_branch_map_view",
			"SELECT map.id, branch.branch, arch.arch, flavor.flavor, config.config, "
				"value "
			"FROM conf_branch_map AS map "
			"LEFT JOIN branch ON map.branch = branch.id "
			"LEFT JOIN config ON map.config = config.id "
			"LEFT JOIN arch ON map.arch = arch.id "
			"LEFT JOIN flavor ON map.flavor = flavor.id;" },
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
		{ "conf_dep_view",
			"SELECT branch.branch, c_parent.config AS parent, c_child.config AS child "
			"FROM conf_dep AS map "
			"LEFT JOIN branch ON map.branch = branch.id "
			"LEFT JOIN config AS c_parent ON map.parent = c_parent.id "
			"LEFT JOIN config AS c_child ON map.child = c_child.id;" },
		{ "module_details_map_view",
			"SELECT map.id, branch.branch, "
				"module_dir.dir || '/' || module.module AS module, "
				"supported "
			"FROM module_details_map AS map "
			"LEFT JOIN module ON map.module = module.id "
			"LEFT JOIN dir AS module_dir ON module.dir = module_dir.id "
			"LEFT JOIN branch ON map.branch = branch.id;" },
		{ "module_file_map_view",
			"SELECT branch.branch, "
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
		{ "ignored_file_branch_map_view",
			"SELECT branch.branch, dir.dir || '/' || file.file AS path "
			"FROM ignored_file_branch_map AS map "
			"LEFT JOIN branch ON map.branch = branch.id "
			"LEFT JOIN file ON map.file = file.id "
			"LEFT JOIN dir ON file.dir = dir.id;" },
		{ "rename_file_version_map_view",
			"SELECT map.version, map.similarity, "
				"olddir.dir || '/' || oldfile.file AS oldpath, "
				"newdir.dir || '/' || newfile.file AS newpath "
			"FROM rename_file_version_map AS map "
			"LEFT JOIN file AS oldfile ON map.oldfile = oldfile.id "
			"LEFT JOIN dir AS olddir ON oldfile.dir = olddir.id "
			"LEFT JOIN file AS newfile ON map.newfile = newfile.id "
			"LEFT JOIN dir AS newdir ON newfile.dir = newdir.id;" },
	};

	return createTables(create_tables) && createIndices(create_indexes) &&
			createViews(create_views);
}

bool F2CSQLConn::prepDB()
{
	const Statements stmts {
		{ insBranch,	"INSERT INTO branch(branch, sha, version) VALUES "
				"(:branch, :sha, :version);" },
		{ insConfig,	"INSERT INTO config(config) VALUES (:config);" },
		{ insArch,	"INSERT INTO arch(arch) VALUES (:arch);" },
		{ insFlavor,	"INSERT INTO flavor(flavor) VALUES (:flavor);" },
		{ insCBMap,	"INSERT INTO conf_branch_map(branch, config, arch, flavor, value) "
					"VALUES ("
					"(SELECT id FROM branch WHERE branch = :branch), "
					"(SELECT id FROM config WHERE config = :config), "
					"(SELECT id FROM arch WHERE arch = :arch), "
					"(SELECT id FROM flavor WHERE flavor = :flavor), "
					":value);" },
		{ insDir,	"INSERT INTO dir(dir) VALUES (:dir);" },
		{ insFile,	"INSERT INTO file(file, dir) VALUES ("
					":file, "
					"(SELECT id FROM dir WHERE dir = :dir));" },
		{ insCFMap,	"INSERT INTO conf_file_map(branch, config, file) VALUES ("
					"(SELECT id FROM branch WHERE branch = :branch), "
					"(SELECT id FROM config WHERE config = :config), "
					"(SELECT id FROM file WHERE file = :file AND "
					"dir = (SELECT id FROM dir WHERE dir = :dir)));" },
		{ insConfDep,	"INSERT INTO conf_dep(branch, parent, child) VALUES ("
					"(SELECT id FROM branch WHERE branch = :branch), "
					"(SELECT id FROM config WHERE config = :parent), "
					"(SELECT id FROM config WHERE config = :child));" },
		{ insModule,	"INSERT INTO module(dir, module) VALUES ("
					"(SELECT id FROM dir WHERE dir = :dir), "
					":module);" },
		{ insMDMap,	"INSERT INTO module_details_map(branch, module, supported) VALUES ("
					"(SELECT id FROM branch WHERE branch = :branch), "
					"(SELECT id FROM module WHERE module = :module AND "
					"dir = (SELECT id FROM dir WHERE dir = :module_dir)), "
					":supported);" },
		{ insMFMap,	"INSERT INTO module_file_map(branch, module, file) VALUES ("
					"(SELECT id FROM branch WHERE branch = :branch), "
					"(SELECT id FROM module WHERE module = :module AND "
					"dir = (SELECT id FROM dir WHERE dir = :module_dir)), "
					"(SELECT id FROM file WHERE file = :file AND "
					"dir = (SELECT id FROM dir WHERE dir = :dir)));" },
		{ insUser,	"INSERT INTO user(email) VALUES (:email);" },
		{ insUFMap,	"INSERT INTO user_file_map(user, branch, file, count, count_no_fixes) "
					"VALUES ("
					"(SELECT id FROM user WHERE email = :email), "
					"(SELECT id FROM branch WHERE branch = :branch), "
					"(SELECT id FROM file WHERE file = :file AND "
					"dir = (SELECT id FROM dir WHERE dir = :dir)), "
					":count, :countnf);" },
		{ insIFBMap,	"INSERT INTO ignored_file_branch_map(branch, file) VALUES ("
					"(SELECT id FROM branch WHERE branch = :branch), "
					"(SELECT id FROM file WHERE file = :file AND "
					"dir = (SELECT id FROM dir WHERE dir = :dir)));" },
		{ insRFVMap,	"INSERT INTO rename_file_version_map(version, similarity, oldfile, newfile) "
					"VALUES (:version, :similarity, "
					"(SELECT id FROM file WHERE file = :oldfile AND "
					"dir = (SELECT id FROM dir WHERE dir = :olddir)), "
					"(SELECT id FROM file WHERE file = :newfile AND "
					"dir = (SELECT id FROM dir WHERE dir = :newdir)));" },
		{ delBranch,	"DELETE FROM branch WHERE branch = :branch;" },
		{ selBranch,	"SELECT 1 FROM branch WHERE branch = :branch;" },
	};

	return prepareStatements(stmts);
}

bool F2CSQLConn::insertBranch(const std::string &branch, const std::string &sha, unsigned version)
{
	return insert(insBranch, {
			      { ":branch", branch },
			      { ":sha", sha },
			      { ":version", version },
		      });
}

bool F2CSQLConn::insertConfig(const std::string &config)
{
	return insert(insConfig, { { ":config", config } });
}

bool F2CSQLConn::insertArch(const std::string &arch)
{
	return insert(insArch, { { ":arch", arch } });
}

bool F2CSQLConn::insertFlavor(const std::string &flavor)
{
	return insert(insFlavor, { { ":flavor", flavor } });
}

bool F2CSQLConn::insertCBMap(const std::string &branch, const std::string &arch,
			     const std::string &flavor, const std::string &config,
			     const std::string &value)
{
	return insert(insCBMap, {
			      { ":branch", branch },
			      { ":arch", arch },
			      { ":flavor", flavor },
			      { ":config", config },
			      { ":value", value },
		      });
}

bool F2CSQLConn::insertDir(const std::string &dir)
{
	return insert(insDir, { { ":dir", dir } });
}

bool F2CSQLConn::insertFile(const std::string &dir, const std::string &file)
{
	return insert(insFile, {
			      { ":dir", dir },
			      { ":file", file },
		      });
}

std::optional<std::pair<std::string, std::string>>
F2CSQLConn::insertPath(const std::filesystem::path &path)
{
	auto dir = path.parent_path();
	auto file = path.filename();

	if (!insertDir(dir))
		return std::nullopt;
	if (!insertFile(dir, file))
		return std::nullopt;

	return std::make_pair(dir.string(), file.string());
}

bool F2CSQLConn::insertCFMap(const std::string &branch, const std::string &config,
			     const std::string &dir, const std::string &file)
{
	return insert(insCFMap, {
			      { ":branch", branch },
			      { ":config", config },
			      { ":dir", dir },
			      { ":file", file },
		      });
}

bool F2CSQLConn::insertConfDep(const std::string &branch, const std::string &parent,
			       const std::string &child)
{
	return insert(insConfDep, {
			      { ":branch", branch },
			      { ":parent", parent },
			      { ":child", child },
		      });
}

bool F2CSQLConn::insertModule(const std::string &dir, const std::string &module)
{
	return insert(insModule, {
			      { ":dir", dir },
			      { ":module", module }
		      });
}

bool F2CSQLConn::insertMDMap(const std::string &branch, const std::string &module_dir,
			     const std::string &module, int supported)
{
	return insert(insMDMap, {
			      { ":branch", branch },
			      { ":module_dir", module_dir },
			      { ":module", module },
			      { ":supported", supported },
		      });
}

bool F2CSQLConn::insertMFMap(const std::string &branch, const std::string &module_dir,
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

bool F2CSQLConn::insertUser(const std::string &email)
{
	return insert(insUser, { { ":email", email } });
}

bool F2CSQLConn::insertUFMap(const std::string &branch, const std::string &email,
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

bool F2CSQLConn::insertIFBMap(const std::string &branch, const std::string &dir,
			     const std::string &file)
{
	return insert(insIFBMap, {
			      { ":branch", branch },
			      { ":dir", dir },
			      { ":file", file },
		      });
}

bool F2CSQLConn::insertRFVMap(unsigned version, unsigned similarity,
			      const std::string &olddir, const std::string &oldfile,
			      const std::string &newdir, const std::string &newfile)
{
	return insert(insRFVMap, {
			      { ":version", version },
			      { ":similarity", similarity },
			      { ":olddir", olddir },
			      { ":oldfile", oldfile },
			      { ":newdir", newdir },
			      { ":newfile", newfile },
		      });
}

bool F2CSQLConn::deleteBranch(const std::string &branch)
{
	return insert(delBranch, { { ":branch", branch } });
}

bool F2CSQLConn::hasBranch(const std::string &branch)
{
	const auto res = select(selBranch, { { ":branch", branch } });
	if (!res)
		RunEx("Cannot select branch: ") << lastError() << raise;

	return res->size() && std::get<int>((*res)[0][0]) == 1;
}
