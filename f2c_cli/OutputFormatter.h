// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <iostream>
#include <string>
#include <sstream>

#include <nlohmann/json.hpp>

namespace F2C {

class OutputFormatter {
public:
	OutputFormatter() {}

	virtual void newObj(const std::string &/*type*/, const std::string &/*value*/) {}
	virtual void addConfig(const std::filesystem::path &path, const std::string &config,
			       const std::filesystem::path &module) = 0;
	virtual void addRename(const std::filesystem::path &oldPath,
			       const std::filesystem::path &newPath,
			       unsigned similarity) = 0;

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

	virtual void addConfig(const std::filesystem::path &path, const std::string &config,
			       const std::filesystem::path &module) override {
		m_json.back()["configs"].push_back({
			{ "path", path.string() },
			{ "config", config },
			{ "module", module }
		});
	}

	virtual void addRename(const std::filesystem::path &oldPath,
			       const std::filesystem::path &newPath,
			       unsigned similarity) override {
		m_json.back()["rename"] = {
			{ "similarity", similarity },
			{ "from", oldPath.string() },
			{ "to", newPath.string() },
		};
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
	OutputFormatterSimple(bool modules) : m_modules(modules) {}

	virtual void addConfig(const std::filesystem::path &path, const std::string &config,
			       const std::filesystem::path &module) override {
		m_configs << path.string() << ' ' << config;
		if (m_modules)
			  m_configs << module.string();
		m_configs << '\n';
	}

	virtual void addRename(const std::filesystem::path &oldPath,
			       const std::filesystem::path &newPath,
			       unsigned similarity) override {
		m_renames << similarity << ' ' << oldPath.string() << ' ' << newPath.string()
			  << '\n';
	}

	virtual void print() const override {
		std::cout << m_configs.str() << m_renames.str();
	}
private:
	std::ostringstream m_configs;
	std::ostringstream m_renames;
	bool m_modules;
};

}
