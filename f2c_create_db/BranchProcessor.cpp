// SPDX-License-Identifier: GPL-2.0-only

#include <nlohmann/json.hpp>

#include <sl/helpers/Color.h>
#include <sl/helpers/Exception.h>
#include <sl/helpers/Process.h>
#include <sl/helpers/PushD.h>
#include <sl/kerncvs/CollectConfigs.h>
#include <sl/kerncvs/PatchesAuthors.h>

#include "parser/kconfig/Parser.h"
#include "treewalker/SQLiteMakeVisitor.h"
#include "treewalker/TreeWalker.h"
#include "Ignores.h"
#include "Verbose.h"

#include "BranchProcessor.h"

using Clr = SlHelpers::Color;
using RunEx = SlHelpers::RuntimeException;
using SlHelpers::raise;

using namespace F2C;

auto BranchProcessor::getSupported(const SlGit::Commit &commit)
{
	auto suppConf = commit.catFile("supported.conf");
	if (!suppConf)
		RunEx("Cannot obtain supported.conf: ") << commit.repo().lastError() << raise;

	return SlKernCVS::SupportedConf { *suppConf };
}

SlGit::Commit BranchProcessor::checkout()
{
	m_notifier.notify("Checking out");
	if (!m_repo.checkout("refs/remotes/origin/" + m_branch))
		RunEx("Cannot check out '") << m_branch << "': " << m_repo.lastError() <<
			raise;

	auto commit = m_repo.commitRevparseSingle("HEAD");
	if (!commit)
		RunEx("Cannot find HEAD: ") << m_repo.lastError() << raise;

	return std::move(*commit);
}

void BranchProcessor::expand()
{
	auto kernelSource = m_scratchArea / "kernel-source";
	std::error_code ec;
	SlHelpers::PushD push(kernelSource, ec);
	if (ec)
		RunEx(__func__) << ": cannot chdir to " << kernelSource << raise;

	m_notifier.notify("Expanding");

	std::filesystem::path seqPatch{"./scripts/sequence-patch"};
	// temporary for old branches
	if (!std::filesystem::exists(seqPatch)) {
		Clr(Clr::YELLOW) << "Running old sequence-patch.sh as sequence-patch does not exist";
		seqPatch = "./scripts/sequence-patch.sh";
	}
	const std::vector<std::string> args {
		"--dir=" + m_scratchArea.string(),
		"--patch-dir=" + m_expandedDir.string(),
		"--rapid",
	};
	SlHelpers::Process P;
	auto ret = P.run(seqPatch, args);
	if (F2C::verbose > 1)
		std::cout << "cmd=" << seqPatch << " stat=" << P.lastErrorNo() << '/' <<
			     P.exitStatus() << '\n';
	if (!ret || P.exitStatus())
		RunEx(__func__) << ": cannot seq patch: " << P.lastError() <<
				   " (" << P.exitStatus() << ')' << raise;
}

void BranchProcessor::insertConfigSQL(const Kconfig::Parser &p)
{
	p.walkConfigs([this](auto conf, auto type) {
		auto realConf = "CONFIG_" + conf;
		if (!m_sql.insertConfig(realConf, static_cast<unsigned>(type)))
			RunEx("Cannot insert config '") << conf << "': " << m_sql.lastError() <<
							   raise;
		m_configs.emplace(std::move(realConf), type);
	});
}

void BranchProcessor::parseKconfigs()
{
	Kconfig::Parser p;
	const auto excludeDir = m_expandedDir / "scripts" / "kconfig" / "tests";
	const auto excludePath = m_expandedDir / "scripts" / "Kconfig.include";

	for (const auto &e: Kconfig::ConfigRange{}) {
		std::string name(Kconfig::Config::getName(e));
		if (!m_sql.insertConfigType(static_cast<unsigned>(e), name))
			RunEx("Cannot insert config type '") << name << "': " <<
							       m_sql.lastError() << raise;
	}

	for (auto it = std::filesystem::recursive_directory_iterator(m_expandedDir);
	     it != std::filesystem::end(it); ++it) {
		const auto &path = it->path();
		if (it->is_directory()) {
			if (path == excludeDir)
				it.disable_recursion_pending();
			continue;
		}
		if (!it->is_regular_file())
			continue;
		if (path.stem() != "Kconfig" && path.stem() != "Kconfig-nommu")
			continue;
		if (path == excludePath)
			continue;

		if (!p.parse(path, false))
			RunEx("Cannot parse: ") << path << raise;

		insertConfigSQL(p);
	}
}

void BranchProcessor::parseKbuilds(const SlKernCVS::SupportedConf &supp)
{
	TW::SQLiteMakeVisitor visitor{ m_sql, supp, m_branch, m_expandedDir, m_configs };
	TW::TreeWalker tw(m_expandedDir, m_configs, visitor);
	tw.walk();
}

void BranchProcessor::processF2C(const SlKernCVS::SupportedConf &supp)
{
	parseKconfigs();
	parseKbuilds(supp);
}

void BranchProcessor::processAuthors(const SlGit::Commit &commit)
{
	SlKernCVS::PatchesAuthors PA{ m_repo, m_opts.authorsDumpRefs,
		m_opts.authorsReportUnhandled };

	auto ret = PA.processAuthors(commit, [this](const std::string &email) -> bool {
		return m_sql.insertUser(email);
	}, [this](const std::string &email, std::filesystem::path &&path,
			unsigned count, unsigned realCount) -> bool {
		auto fileDir = m_sql.insertPath(path);
		return fileDir && m_sql.insertUFMap(m_branch, email, std::move(fileDir->first),
						    std::move(fileDir->second),
						    count, realCount);
	});
	if (!ret)
		RunEx("Cannot process authors").raise();
}

void BranchProcessor::processConfigs(const SlGit::Commit &commit)
{
	std::string error;

	SlKernCVS::CollectConfigs CC{ m_repo,
		[this, &error](const std::string &arch, const std::string &flavor) {
			auto ret = m_sql.insertArch(arch) && m_sql.insertFlavor(flavor);
			if (!ret)
				error = m_sql.lastError();
			return ret;
		}, [this, &error](const std::string &arch,
				  const std::string &flavor,
				  std::string &&config,
				  const SlKernCVS::CollectConfigs::ConfigValue &value) {
			if (!m_configs.contains(config)) {
				Clr(std::cerr, Clr::YELLOW) << "config \"" << config <<
							       "\" is not defined (" << arch <<
							       '/' << flavor << ')';
				return true;
			}
			auto ret = m_sql.insertCBMap(m_branch, arch, flavor, config,
						     std::string(1, value));
			if (!ret)
				error = m_sql.lastError();
			return ret;
	}};

	if (!CC.collectConfigs(commit))
		RunEx("Cannot collect configs: ") << error << raise;
}

void BranchProcessor::processInternal(SlGit::Commit &commit)
{
	m_sql.begin();
	auto SHA = commit.idStr();
	BranchProps props{ commit };

	if (!m_sql.insertBranch(m_branch, SHA, props.version))
		RunEx("Cannot add branch '") << m_branch << "' with SHA '" << SHA << '\'' <<
						raise;

	m_branchesProps.emplace(m_branch, std::move(props));

	if (!m_opts.sqliteCreateOnly) {
		m_notifier.notify("Retrieving supported info");
		auto supp = getSupported(commit);

		m_notifier.notify("Running file2config");
		processF2C(supp);

		m_notifier.notify("Collecting configs");
		processConfigs(commit);

		m_notifier.notify("Detecting authors of patches");
		processAuthors(commit);

		if (m_configuration) {
			m_notifier.notify("Collecting ignored files");
			Ignores::process(m_sql, m_branch, *m_configuration, m_expandedDir);
		}
	}

	m_notifier.notify("Committing");
	m_sql.end();
}
