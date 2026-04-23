// SPDX-License-Identifier: GPL-2.0-only

#include <iostream>
#include <string_view>
#include <utility>

#include <sl/helpers/Color.h>
#include <sl/helpers/Exception.h>
#include <sl/helpers/String.h>
#include <sl/helpers/Views.h>

#include "../parser/make/EntryVisitor.h"
#include "MakeVisitor.h"
#include "TreeWalker.h"
#include "../Verbose.h"

using namespace TW;
using Clr = SlHelpers::Color;
using RunEx = SlHelpers::RuntimeException;
using SlHelpers::raise;

void TreeWalker::forEachSubDir(const std::filesystem::path &dir,
			       const std::function<void(const std::filesystem::path &entry)> &CB)
{
	std::error_code ec;

	const auto iter = std::filesystem::directory_iterator{dir, ec};
	if (ec) {
		std::cerr << __func__ << ": " << dir << " not found!\n";
		return;
	}
	for (const auto &entry : iter)
		if (entry.is_directory()) {
			CB(entry.path());
		}
}

void TreeWalker::addDefaultKernelFiles(CondStack s, const std::filesystem::path &start)
{
	// skip these
	m_visitedMakefiles.emplace(start/"scripts/Kbuild.include");
	m_visitedMakefiles.emplace(start/"scripts/Makefile.gcc-plugins");

	// start with top-level Makefile
	appendToWalk(s, start/"Makefile");
	// and it includes Kbuild
	appendToWalk(s, start/"Kbuild");

	forEachSubDir(start/"arch", [this](const std::filesystem::path &path) {
		archs.emplace_back(path.stem());
	});

	forEachSubDir(start/"arch/arm", [this, &s](const std::filesystem::path &path) {
		static constexpr const std::string_view lookingFor[] { "mach-", "plat-" };
		const auto stem = path.stem().string();
		for (const auto &lf: lookingFor)
			if (stem.starts_with(lf)) {
				auto makefile = path/"Makefile";
				if (std::filesystem::exists(makefile))
					appendToWalk(s, std::move(makefile));
			}
	});

	auto s390Boot = start/"arch/s390/boot/Makefile";
	if (std::filesystem::exists(s390Boot))
		appendToWalk(std::move(s), std::move(s390Boot));
}

TreeWalker::TreeWalker(const std::filesystem::path &start, const Kconfig::Config::Configs &configs,
		       const MakeVisitor &makeVisitor) :
	m_configs(configs), makeVisitor(makeVisitor), start(start)
{
	CondStack s { "y" };

	if (std::filesystem::exists(start/"Documentation"))
		addDefaultKernelFiles(std::move(s), start);
	else
		addDirectory(start, std::move(s), start);
}

void TreeWalker::addTargetEntry(CondStack s,
				const std::filesystem::path &objPath,
				std::string cond,
				const std::string &entry)
{
	if (F2C::verbose > 1)
		std::cout << __func__ << ": cond=" << cond << " e=" << entry << '\n';

	s.emplace_back(std::move(cond));
	auto module = objPath;
	module.replace_extension();
	handleObject(std::move(s), objPath.parent_path() / entry, module);
}

/**
 * @brief Find sources for \p objPath
 *
 * @param s Condition stack
 * @param objPath Object to find sources of
 * @return true on success
 *
 * \p objPath (module) is composed of more sources, so the AST needs to be
 * walked recursively to find all the sources.
 */
bool TreeWalker::tryHandleTarget(CondStack s, const std::filesystem::path &objPath)
{
	auto lookingFor = objPath.stem().string() + "-";

	if (F2C::verbose > 1) {
		std::cout << __func__ << ": obj=" << objPath << " lookingFor=" <<
			     lookingFor << " cond=";
		for (const auto &e: s)
			std::cout << e << ",";
		std::cout << "]\n";
	}

	bool found = false;

	class TargetVisitor : public MP::EntryVisitor {
	public:
		TargetVisitor(TreeWalker &TW, const CondStack &s,
			      const std::filesystem::path &objPath,
			      std::string_view lookingFor, bool &found)
			: TW(TW), s(s), objPath(objPath), lookingFor(lookingFor), found(found) {}

		virtual const std::any isInteresting(const std::string &lhs) const override {
			if (!lhs.starts_with(lookingFor))
				return std::any();
			if (F2C::verbose > 1)
				std::cout << "\tSAME PREFIX: " << lookingFor << " == " << lhs << '\n';
			if (lhs[lookingFor.length()] == '$') {
				if (F2C::verbose > 1)
					std::cout << "\t\tMATCH1\n";
				return true;
			}
			for (const auto &s: { "y", "m", "objs" }) {
				if (F2C::verbose > 1)
					std::cout << "\t\ttrying: " <<
						     lhs.substr(lookingFor.length()) <<
						     " against '" << s << "'\n";
				if (!lhs.compare(lookingFor.length(), std::string::npos, s)) {
					if (F2C::verbose > 1)
						std::cout << "\t\tMATCH2: " << s << '\n';
					return true;
				}
			}
			return std::any();
		}

		virtual void entry(const std::any &, const std::string &cond,
				   MP::EntryType type, const std::string &word) const override {
			if (type == MP::EntryType::Object) {
				TW.addTargetEntry(s, objPath, cond, word);
				found = true;
			}
		}
	private:
		TreeWalker &TW;
		const CondStack &s;
		const std::filesystem::path &objPath;
		std::string_view lookingFor;
		bool &found;
	} visitor(*this, s, objPath, lookingFor, found);

	parser.walkAST(archs, visitor, start, objPath.parent_path());

	if (F2C::verbose > 1) {
		std::cout << __func__ << " DONE: obj=" << objPath << " found=" << found << '\n';
	}

	return found;
}

bool TreeWalker::isBuiltIn(const std::string &cond)
{
	// can be empty for unknown vars like ACPI_FUTURE_USAGE
	return cond.empty() || cond == "y" || cond == "m" || cond == "objs";
}

std::optional<std::string> TreeWalker::getCond(const CondStack &s)
{
	for (const auto &e: s | std::views::reverse)
		if (!isBuiltIn(e))
			return e;

	return std::nullopt;
}

std::optional<std::string> TreeWalker::getTristateConf(const CondStack &s)
{
	for (const auto &e: s | std::views::reverse) {
		auto confIt = m_configs.find(e);
		if (confIt == m_configs.end())
			continue;
		if (confIt->second == Kconfig::ConfType::Tristate ||
				confIt->second == Kconfig::ConfType::DefTristate)
			return e;
	}

	return std::nullopt;
}

void TW::TreeWalker::appendToWalk(CondStack s, std::filesystem::path kbPath,
				  std::filesystem::path cwd)
{
	if (!m_visitedMakefiles.insert(kbPath).second) {
		if (F2C::verbose > 1)
			Clr(std::cerr, Clr::YELLOW) << __func__ << ": makefile " << kbPath <<
				" already walked";
		return;
	}
	if (cwd.empty())
		cwd = kbPath.parent_path();
	m_toWalk.emplace(std::move(s), std::move(kbPath), std::move(cwd));
}

/**
 * @brief Handle "obj-X := file.o", see also addRegularEntry()
 *
 * @param s Condition stack
 * @param objPath Object to find sources of
 * @param module Name (path) of the .ko module for \p objPath
 *
 * First, check if this is a simple rule -- one source file per module. If so, it is the short path.
 * If not, tryHandleTarget() needs to find all the sources for the module.
 */
void TreeWalker::handleObject(CondStack s, const std::filesystem::path &objPath,
			      const std::filesystem::path &module)
{
	if (F2C::verbose > 1)
		std::cout << "have OBJ: " << objPath << "\n";

	auto condOpt = getCond(s);
	if (!condOpt)
		return;
	auto cond = std::move(*condOpt);

	if (!visitedPaths.insert(objPath).second) {
		makeVisitor.ignored(objPath, cond);
		return;
	}

	for (const auto &suffix : { ".c", ".S", ".rs" }) {
		auto srcPath = objPath;
		srcPath.replace_extension(suffix);
		if (std::filesystem::exists(srcPath)) {
			makeVisitor.config(srcPath, cond);
			makeVisitor.module(srcPath, module, getTristateConf(s));
			return;
		}
	}

	s.emplace_back(std::move(cond));
	if (!tryHandleTarget(std::move(s), objPath) && F2C::verbose)
		std::cerr << objPath << " source not found\n";
}

/// @brief Handle "obj-X := file.o" or "obj-X := dir/", where X is \p cond and file/dir is \p word
void TreeWalker::addRegularEntry(CondStack s, const std::filesystem::path &kbPath,
				 const std::any &interesting,
				 std::string cond,
				 MP::EntryType type,
				 const std::string &word)
{
	if (type == MP::EntryType::Directory) {
		auto absolute = std::any_cast<bool>(interesting);
		auto dir = absolute ? start / word : kbPath.parent_path() / word;
		if (!visitedDirs.insert(dir).second)
			return;
		if (F2C::verbose > 1)
			std::cout << "pushing dir (" << (absolute ? "abs" : "rela") << "): " <<
				     dir << "\n";
		s.emplace_back(std::move(cond));
		addDirectory(kbPath, std::move(s), dir);
	} else if (type == MP::EntryType::Object) {
		s.emplace_back(std::move(cond));
		auto obj = kbPath.parent_path() / word;
		auto module = obj;
		module.replace_extension();
		handleObject(std::move(s), obj, module);
	}
}

/// @brief Handle one queued Kbuild file
void TreeWalker::handleKbuildFile(const ToWalkEntry &entry)
{
	if (F2C::verbose > 1)
		std::cout << __func__ << ": " << entry.kbPath << "\n";

	if (!parser.parse(entry.kbPath))
		RunEx("cannot parse ") << entry.kbPath << raise;

	class RegularVisitor : public MP::EntryVisitor {
	public:
		RegularVisitor(TreeWalker &TW, const ToWalkEntry &entry)
			: TW(TW), m_entry(entry) {}

		virtual const std::any isInteresting(const std::string &lhs) const override {
			 static constexpr const std::pair<std::string_view, bool> lookingFor[] = {
				 { "lib-", false },
				 { "obj-", false },
				 { "subdir-", false },
				 { "platform-", false },
				 { "core-", true },
				 { "drivers-", true },
				 { "libs-", true },
				 { "net-", true },
				 { "virt-", true },
			 };

			 for (const auto &LF: lookingFor)
				 if (lhs.starts_with(LF.first))
					 return LF.second;

			 return std::any();
		}

		virtual void entry(const std::any &interesting, const std::string &cond,
				   MP::EntryType type, const std::string &word) const override {
			TW.addRegularEntry(m_entry.cs, m_entry.kbPath, interesting, cond, type,
					   word);
		}

		virtual void include(const std::filesystem::path &dest) const override {
			TW.appendToWalk(m_entry.cs, dest, m_entry.cwd);
		}
	private:
		TreeWalker &TW;
		const ToWalkEntry &m_entry;
	} visitor(*this, entry);

	parser.walkAST(archs, visitor, start, entry.cwd);
}

/// @brief Find Kbuild or Makefile in @p path and add it to the queue
void TreeWalker::addDirectory(const std::filesystem::path &kbPath, CondStack s,
			      const std::filesystem::path &path)
{
	if (F2C::verbose > 1) {
		std::cout << __func__ << ": path=" << path << " cond=[";
		for (const auto &e: s)
			std::cout << e << ",";
		std::cout << "]\n";
	}

	for (const auto &kb_file: { "Kbuild", "Makefile" }) {
		if (std::filesystem::exists(path / kb_file)) {
			appendToWalk(std::move(s), path / kb_file);
			return;
		}
	}

	if (!F2C::quiet)
		std::cerr << __func__ << ": " << kbPath << ": Kbuild/Makefile not found in " <<
			     path << "\n";
}

/// @brief The top-level function to walk the source tree. The queue is preinitialized in
/// the constructor.
void TreeWalker::walk()
{
	for (; !m_toWalk.empty(); m_toWalk.pop())
		handleKbuildFile(m_toWalk.front());
}
