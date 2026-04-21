// SPDX-License-Identifier: GPL-2.0-only

#include <iostream>
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

void TreeWalker::addDefaultKernelFiles(const CondStack &s, const std::filesystem::path &start)
{
	// start with top-level Makefile
	appendToWalk(s, start/"Makefile");
	// and it includes Kbuild
	appendToWalk(s, start/"Kbuild");

	forEachSubDir(start/"arch", [this, &s](const std::filesystem::path &path) {
		archs.push_back(path.stem());
		// we do not handle 'include's, so do what top-level 'Makefile' does
		appendToWalk(s, path/"Makefile");
	});

	forEachSubDir(start/"arch/arm", [this, &s](const std::filesystem::path &path) {
		static const std::string lookingFor[] { "mach-", "plat-" };
		const auto stem = path.stem().string();
		for (const auto &lf: lookingFor)
			if (!stem.compare(0, lf.length(), lf)) {
				auto makefile = path/"Makefile";
				if (std::filesystem::exists(makefile))
					appendToWalk(s, std::move(makefile));
			}
	});

	auto mipsPlat = start/"arch/mips/Kbuild.platforms";
	if (std::filesystem::exists(mipsPlat))
		appendToWalk(s, std::move(mipsPlat));

	auto s390Boot = start/"arch/s390/boot/Makefile";
	if (std::filesystem::exists(s390Boot))
		appendToWalk(s, std::move(s390Boot));
}

TreeWalker::TreeWalker(const std::filesystem::path &start, const Kconfig::Config::Configs &configs,
		       const MakeVisitor &makeVisitor) :
	m_configs(configs), makeVisitor(makeVisitor), start(start)
{
	CondStack s;
	s.push_back("y");

	if (std::filesystem::exists(start/"Documentation"))
		addDefaultKernelFiles(s, start);
	else
		addDirectory(start, s, start);

	if (F2C::verbose) {
		std::cout << __func__ << ": start=";
		for (const auto &e: m_toWalk)
			std::cout << e.kbPath << ",";
		std::cout << "]\n";
	}
}

void TreeWalker::addTargetEntry(const CondStack &s,
				const std::filesystem::path &objPath,
				const std::string &cond,
				const MP::EntryType &type,
				const std::string &entry, bool &found)
{
	if (F2C::verbose > 1)
		std::cout << __func__ << ": cond=" << cond << " t=" << type << " e=" << entry << '\n';

	if (type == MP::EntryType::Object) {
		auto newS(s);
		newS.push_back(cond);
		auto module = objPath;
		module.replace_extension();
		handleObject(newS, objPath.parent_path() / entry, module);
		found = true;
	}
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
bool TreeWalker::tryHandleTarget(const CondStack &s, const std::filesystem::path &objPath)
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
		TargetVisitor(TreeWalker &TW, const CondStack &s, const std::filesystem::path &objPath,
			 const std::string &lookingFor, bool &found)
			: TW(TW), s(s), objPath(objPath), lookingFor(lookingFor), found(found) {}

		virtual const std::any isInteresting(const std::string &lhs) const override {
			if (lhs.compare(0, lookingFor.length(), lookingFor))
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
				   const enum MP::EntryType &type, const std::string &word) const override {
			TW.addTargetEntry(s, objPath, cond, type, word, found);
		}
	private:
		TreeWalker &TW;
		const CondStack &s;
		const std::filesystem::path &objPath;
		const std::string &lookingFor;
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

void TW::TreeWalker::appendToWalk(CondStack s, std::filesystem::path kbPath)
{
	if (!m_visitedMakefiles.insert(kbPath).second) {
		if (F2C::verbose > 1)
			Clr(std::cerr, Clr::YELLOW) << __func__ << ": makefile " << kbPath <<
				" already walked";
		return;
	}
	m_toWalk.emplace_back(std::move(s), std::move(kbPath));
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
void TreeWalker::handleObject(const CondStack &s, const std::filesystem::path &objPath,
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

	auto newS(s);
	newS.push_back(cond);
	if (!tryHandleTarget(newS, objPath) && F2C::verbose)
		std::cerr << objPath << " source not found\n";
}

/// @brief Handle "obj-X := file.o" or "obj-X := dir/", where X is \p cond and file/dir is \p word
void TreeWalker::addRegularEntry(const CondStack &s, const std::filesystem::path &kbPath,
				 const std::any &interesting,
				 const std::string &cond,
				 const enum MP::EntryType &type,
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
		auto newS(s);
		newS.push_back(cond);
		addDirectory(kbPath, newS, dir);
	} else if (type == MP::EntryType::Object) {
		auto newS(s);
		newS.push_back(cond);
		auto obj = kbPath.parent_path() / word;
		auto module = obj;
		module.replace_extension();
		handleObject(newS, obj, module);
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
			 static const std::pair<std::string, bool> lookingFor[] = {
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
				 if (!lhs.compare(0, LF.first.length(), LF.first))
					 return LF.second;

			 return std::any();
		}

		virtual void entry(const std::any &interesting, const std::string &cond,
				   const enum MP::EntryType &type,
				   const std::string &word) const override {
			TW.addRegularEntry(m_entry.cs, m_entry.kbPath, interesting, cond, type,
					   word);
		}
	private:
		TreeWalker &TW;
		const ToWalkEntry &m_entry;
	} visitor(*this, entry);

	parser.walkAST(archs, visitor, start, entry.kbPath.parent_path());
}

/// @brief Find Kbuild or Makefile in @p path and add it to the queue
void TreeWalker::addDirectory(const std::filesystem::path &kbPath, const CondStack &s,
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
			appendToWalk(s, path / kb_file);
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
	while (!m_toWalk.empty()) {
		auto top = m_toWalk.back();
		m_toWalk.pop_back();
		handleKbuildFile(top);
	}
}
