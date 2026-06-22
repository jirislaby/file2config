// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>

#include <nlohmann/json_fwd.hpp>

#include <sl/git/Commit.h>
#include <sl/git/Repo.h>
#include <sl/helpers/String.h>
#include <sl/kerncvs/SupportedConf.h>

#include "BranchProps.h"
#include "Configs.h"
#include "F2CSQLConn.h"
#include "Opts.h"
#include "StatusNotifier.h"
#include "parser/kconfig/Config.h"

namespace Kconfig {
	class Parser;
}

namespace F2C {

class BranchProcessor {
	using Json = nlohmann::ordered_json;
public:
	using UserSet = std::unordered_set<std::string, SlHelpers::String::Hash,
	      SlHelpers::String::Eq>;

	BranchProcessor() = delete;

	BranchProcessor(const std::string &branch,
			const StatusNotifier &notifier,
			const std::filesystem::path &scratchArea,
			BranchesProps &branchesProps,
			const SlGit::Repo &repo,
			F2CSQLConn &sql,
			const Opts &opts,
			const std::optional<Json> &configuration,
			const UserSet &validUsers) :
		m_branch(branch), m_notifier(notifier), m_scratchArea(scratchArea),
		m_expandedDir(getExpandedDir()), m_branchesProps(branchesProps),
		m_repo(repo), m_sql(sql), m_opts(opts), m_configuration(configuration),
		m_validUsers(validUsers) { }

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

	Kconfig::Config::Configs parseKconfigs();
	void insertConfigSQL(const Kconfig::Parser &p, Kconfig::Config::Configs &configs);
	static void addConfig(EnabledConfigMap &enabledConfigs, const std::string &key,
			      SlKernCVS::ConfigValue newVal);
	EnabledConfigMap processConfigs(const SlGit::Commit &commit,
					const Kconfig::Config::Configs &configs);
	void parseKbuilds(const SlKernCVS::SupportedConf &supp,
			  const Kconfig::Config::Configs &configs,
			  const EnabledConfigMap &enabledConfigs);

	bool isValidUser(std::string_view email);
	void processAuthors(const SlGit::Commit &commit);
	void processInternal(SlGit::Commit &commit);

	const std::string &m_branch;
	const StatusNotifier &m_notifier;
	const std::filesystem::path &m_scratchArea;
	std::filesystem::path m_expandedDir;
	BranchesProps &m_branchesProps;
	const SlGit::Repo &m_repo;
	F2CSQLConn &m_sql;
	const Opts &m_opts;
	const std::optional<Json> &m_configuration;
	const UserSet &m_validUsers;
};

} // namespace
