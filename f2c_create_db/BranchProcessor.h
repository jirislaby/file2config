// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include <nlohmann/json_fwd.hpp>

#include <sl/git/Commit.h>
#include <sl/git/Repo.h>
#include <sl/kerncvs/SupportedConf.h>

#include "BranchProps.h"
#include "Opts.h"
#include "StatusNotifier.h"
#include "parser/kconfig/Config.h"
#include "sql/F2CSQLConn.h"

namespace Kconfig {
	class Parser;
}

namespace F2C {

class BranchProcessor {
	using Json = nlohmann::ordered_json;
public:
	BranchProcessor() = delete;

	BranchProcessor(const std::string &branch,
			const StatusNotifier &notifier,
			const std::filesystem::path &scratchArea,
			BranchesProps &branchesProps,
			const SlGit::Repo &repo,
			SQL::F2CSQLConn &sql,
			const Opts &opts,
			const std::optional<Json> &configuration) :
		m_branch(branch), m_notifier(notifier), m_scratchArea(scratchArea),
		m_expandedDir(getExpandedDir()), m_branchesProps(branchesProps),
		m_repo(repo), m_sql(sql), m_opts(opts), m_configuration(configuration) { }

	void process() {
		auto commit = checkout();
		expand();
		processInternal(commit);
	}
private:
	std::filesystem::path getExpandedDir() {
		std::string branch = m_branch;
		std::replace(branch.begin(), branch.end(), '/', '_');
		return m_scratchArea / branch;
	}
	static auto getSupported(const SlGit::Commit &commit);

	SlGit::Commit checkout();
	void expand();
	void insertConfigSQL(const Kconfig::Parser &p);
	void parseKconfigs();
	void parseKbuilds(const SlKernCVS::SupportedConf &supp);
	void processF2C(const SlKernCVS::SupportedConf &supp);
	void processAuthors(const SlGit::Commit &commit);
	void processConfigs(const SlGit::Commit &commit);
	void processInternal(SlGit::Commit &commit);

	const std::string &m_branch;
	const StatusNotifier &m_notifier;
	const std::filesystem::path &m_scratchArea;
	std::filesystem::path m_expandedDir;
	BranchesProps &m_branchesProps;
	const SlGit::Repo &m_repo;
	SQL::F2CSQLConn &m_sql;
	const Opts &m_opts;
	const std::optional<Json> &m_configuration;
	Kconfig::Config::Configs m_configs;
};

} // namespace
