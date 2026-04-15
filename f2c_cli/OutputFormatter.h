// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <iostream>
#include <string>
#include <sstream>
#include <string_view>

#include <nlohmann/json.hpp>

namespace F2C {

class OutputFormatter {
public:
	OutputFormatter() {}
	virtual ~OutputFormatter() = default;

	virtual void newObj(const std::string &/*type*/, const std::string &/*value*/) {}
	virtual void addConfig(const std::filesystem::path &path, const std::string &branch,
			       const std::string &config,
			       const std::filesystem::path &module) = 0;
	virtual void addConfigDetails(const bool forModules, const std::string &arch,
				      const std::string &flavor,
				      const std::string &value) = 0;
	virtual void addRename(const std::filesystem::path &oldPath,
			       const std::filesystem::path &newPath,
			       const std::string &branch,
			       unsigned similarity) = 0;
	virtual void addModule(const std::filesystem::path &path, const std::string &branch,
			       int supported, std::string_view config) = 0;
	virtual void addModuleFile(const std::filesystem::path &file) = 0;

	virtual void print() const = 0;
protected:
};

class OutputFormatterJSON : public OutputFormatter {
public:
	using Json = nlohmann::ordered_json;

	OutputFormatterJSON() {}

	virtual void newObj(const std::string &type, const std::string &value) override {
		m_json.emplace_back(Json::object({ { "query", {
			{ "type", type },
			{ "value", value },
		}}}));
	}

	virtual void addConfig(const std::filesystem::path &path, const std::string &branch,
			       const std::string &config,
			       const std::filesystem::path &module) override {
		m_json.back()["configs"].push_back({
			{ "branch", branch },
			{ "path", path.string() },
			{ "config", config },
			{ "module", module.filename() },
			{ "module_path", module },
		});
	}
	virtual void addConfigDetails(const bool forModules, const std::string &arch,
				      const std::string &flavor,
				      const std::string &value) override {
		auto &last = m_json.back()[forModules ? "modules" : "configs"].back();
		last["config_values"][arch][flavor] = value;
	}

	virtual void addRename(const std::filesystem::path &oldPath,
			       const std::filesystem::path &newPath,
			       const std::string &branch,
			       unsigned similarity) override {
		m_json.back()["renames"].push_back({
			{ "branch", branch },
			{ "similarity", similarity },
			{ "from", oldPath.string() },
			{ "to", newPath.string() },
		});
	}

	virtual void addModule(const std::filesystem::path &path, const std::string &branch,
			       int supported, std::string_view config) override {
		m_json.back()["modules"].push_back({
			{ "branch", branch },
			{ "path", path },
			{ "supported", supported },
			{ "config", config },
		});
	}

	virtual void addModuleFile(const std::filesystem::path &file) override {
		m_json.back()["modules"].back()["files"].emplace_back(file);
	}

	virtual void print() const override {
		std::cout << std::setw(2) << m_json << '\n';
	}
private:
	Json m_json;
};

class OutputFormatterSimple : public OutputFormatter {
public:
	OutputFormatterSimple() = delete;
	OutputFormatterSimple(bool modules, bool multipleBranches)
		: m_modules(modules), m_multipleBranches(multipleBranches) {}

	virtual void addConfig(const std::filesystem::path &path, const std::string &branch,
			       const std::string &config,
			       const std::filesystem::path &module) override {
		if (m_multipleBranches)
			m_configs << branch << ' ';
		m_configs << path.string() << ' ' << config;
		if (m_modules)
			  m_configs << ' ' << module.string();
		m_configs << '\n';
	}

	virtual void addConfigDetails(const bool /*forModules*/, const std::string &/*arch*/,
				      const std::string &/*flavor*/,
				      const std::string &/*value*/) override { }

	virtual void addRename(const std::filesystem::path &oldPath,
			       const std::filesystem::path &newPath,
			       const std::string &branch,
			       unsigned similarity) override {
		if (m_multipleBranches)
			m_renames << branch << ' ';
		m_renames << similarity << ' ' << oldPath.string() << ' ' << newPath.string()
			  << '\n';
	}

	virtual void addModule(const std::filesystem::path &path, const std::string &branch,
			       int supported, std::string_view config) override {
		if (m_multipleBranches)
			m_configs << branch << ' ';
		m_configs << path.string() << ' ' << supported << ' ' << config << '\n';
	}
	virtual void addModuleFile(const std::filesystem::path &file) override {
		m_configs << '\t' << file.string() << '\n';
	}

	virtual void print() const override {
		std::cout << m_configs.str() << m_renames.str();
	}
private:
	std::ostringstream m_configs;
	std::ostringstream m_renames;
	bool m_modules;
	bool m_multipleBranches;
};

}
