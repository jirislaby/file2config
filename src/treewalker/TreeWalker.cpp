// SPDX-License-Identifier: GPL-2.0-only

#include <iostream>

#include "../parser/EntryCallback.h"
#include "MakeVisitor.h"
#include "TreeWalker.h"
#include "../Verbose.h"

using namespace TW;

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
	toWalk.push_back(std::make_pair(s, start/"Makefile"));
	// and it includes Kbuild
	toWalk.push_back(std::make_pair(s, start/"Kbuild"));

	forEachSubDir(start/"arch", [this, &s](const std::filesystem::path &path) {
		archs.push_back(path.stem());
		// we do not handle 'include's, so do what top-level 'Makefile' does
		toWalk.push_back(std::make_pair(s, path/"Makefile"));
	});

	forEachSubDir(start/"arch/arm", [this, &s](const std::filesystem::path &path) {
		static const std::string lookingFor[] { "mach-", "plat-" };
		const auto stem = path.stem().string();
		for (const auto &lf: lookingFor)
			if (!stem.compare(0, lf.length(), lf)) {
				const auto makefile = path/"Makefile";
				if (std::filesystem::exists(makefile))
					toWalk.push_back(std::make_pair(s, makefile));
			}
	});

	auto mipsPlat = start/"arch/mips/Kbuild.platforms";
	if (std::filesystem::exists(mipsPlat))
		toWalk.push_back(std::make_pair(s, mipsPlat));

	auto s390Boot = start/"arch/s390/boot/Makefile";
	if (std::filesystem::exists(s390Boot))
		toWalk.push_back(std::make_pair(s, s390Boot));
}

TreeWalker::TreeWalker(const std::filesystem::path &start, const MakeVisitor &makeVisitor) :
	makeVisitor(makeVisitor), start(start)
{
	CondStack s;
	s.push_back("y");

	if (std::filesystem::exists(start/"Documentation"))
		addDefaultKernelFiles(s, start);
	else
		addDirectory(start, s, start);

	if (F2C::verbose) {
		std::cout << __func__ << ": start=";
		for (const auto &e: toWalk)
			std::cout << e.second << ",";
		std::cout << "]\n";
	}
}

void TreeWalker::addTargetEntry(const CondStack &s, const std::filesystem::path &objPath,
				const std::string &cond,
				const MP::EntryType &type,
				const std::string &entry, bool &found)
{
	if (F2C::verbose > 1)
		std::cout << __func__ << ": cond=" << cond << " t=" << type << " e=" << entry << '\n';

	if (type == MP::EntryType::Object) {
		auto newS(s);
		newS.push_back(cond);

		handleObject(newS, objPath.parent_path() / entry);
		found = true;
	}
}

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

	class TargetEC : public MP::EntryCallback {
	public:
		TargetEC(TreeWalker &TW, const CondStack &s, const std::filesystem::path &objPath,
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
				if (!lhs.compare(lookingFor.length(), ~0U, s)) {
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
	} CB(*this, s, objPath, lookingFor, found);

	parser.walkTree(CB);

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

std::string TreeWalker::getCond(const CondStack &s)
{
	for (auto I = s.rbegin(); I != s.rend(); ++I)
		if (!isBuiltIn(*I))
			return *I;

	return "y";
}

void TreeWalker::handleObject(const CondStack &s, const std::filesystem::path &objPath)
{
	if (F2C::verbose > 1)
		std::cout << "have OBJ: " << objPath << "\n";

	auto cond = getCond(s);
	if (isBuiltIn(cond))
		return;

	if (!visitedPaths.insert(objPath).second) {
		makeVisitor.ignored(objPath, cond);
		return;
	}

	for (const auto &suffix : { ".c", ".S", ".rs" }) {
		auto srcPath = objPath;
		srcPath.replace_extension(suffix);
		if (std::filesystem::exists(srcPath)) {
			makeVisitor.config(srcPath, cond);
			return;
		}
	}

	auto newS(s);
	newS.push_back(cond);
	if (!tryHandleTarget(newS, objPath) && F2C::verbose)
		std::cerr << objPath << " source not found\n";
}

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
		handleObject(newS, kbPath.parent_path() / word);
	}
}

void TreeWalker::handleKbuildFile(const CondStack &s, const std::filesystem::path &kbPath)
{
	if (F2C::verbose > 1)
		std::cout << __func__ << ": " << kbPath << "\n";

	class RegularEC : public MP::EntryCallback {
	public:
		RegularEC(TreeWalker &TW, const CondStack &s, const std::filesystem::path &kbPath)
			: TW(TW), s(s), kbPath(kbPath) {}

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
			TW.addRegularEntry(s, kbPath, interesting, cond, type, word);
		}
	private:
		TreeWalker &TW;
		const CondStack &s;
		const std::filesystem::path &kbPath;
	} CB(*this, s, kbPath);

	parser.parse(archs, kbPath.string(), CB);
}

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
			toWalk.push_back(std::make_pair(s, path / kb_file));
			return;
		}
	}

	std::cerr << __func__ << ": " << kbPath << ": Kbuild/Makefile not found in " << path << "\n";
}

void TreeWalker::walk()
{
	while (!toWalk.empty()) {
		auto top = toWalk.back();
		toWalk.pop_back();
		handleKbuildFile(top.first, top.second);
	}
}
