#include <iostream>

#include "../parser/parser.h"
#include "treewalker.h"

extern unsigned verbose;

using namespace TW;

TreeWalker::TreeWalker(const std::filesystem::path &start)
{
	CondStack s;
	s.push_back("y");
	// start with top-level Makefile
	toWalk.push_back(std::make_pair(s, start/"Makefile"));
	// and it includes Kbuild
	toWalk.push_back(std::make_pair(s, start/"Kbuild"));

	std::error_code ec;
	const auto arch_dir = start/"arch";
	const auto iter = std::filesystem::directory_iterator{arch_dir, ec};
	if (ec) {
		std::cerr << __func__ << ": " << arch_dir << " not found!\n";
		return;
	}
	for (const auto &entry : iter)
		if (entry.is_directory()) {
			archs.push_back(entry.path().stem());
			// we do not handle 'include's, so do what top-level 'Makefile' does
			toWalk.push_back(std::make_pair(s, entry.path()/"Makefile"));
		}
	if (verbose) {
		std::cout << __func__ << ": start=";
		for (const auto &e: toWalk)
			std::cout << e.second << ",";
		std::cout << "]\n";
	}
}

bool TreeWalker::tryHandleTarget(const CondStack &s, const std::filesystem::path &objPath)
{
	if (verbose > 1) {
		std::cout << __func__ << ": obj=" << objPath << " cond=";
		for (const auto &e: s)
			std::cout << e << ",";
		std::cout << "]\n";
	}

	bool found = false;

	parser.findTarget(objPath.stem(),
		[this, &found, &objPath, &s](const std::string &cond,
			  const enum MP::MakeExprListener::EntryType &type,
			  const std::string &entry) {
			if (verbose > 1)
				std::cout << "HERE: cond=" << cond << " t=" << type <<
					     " e=" << entry << '\n';
			if (type == MP::MakeExprListener::Object) {
				auto newS(s);
				newS.push_back(cond);

				handleObject(newS, objPath.parent_path() / entry);
				found = true;
			}
		});

	//self.handle_rule(prefix, path, kb_path, lines, cond, sep, objects)

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
	if (verbose > 1)
		std::cout << "have OBJ: " << objPath << "\n";
	auto cond = getCond(s);
	if (isBuiltIn(cond))
		return;

	for (const auto &suffix : { ".c", ".S", ".rs" }) {
		auto srcPath = objPath;
		srcPath.replace_extension(suffix);
		if (std::filesystem::exists(srcPath)) {
			std::cout << "XXX " << cond << " " << srcPath.string() << "\n";
			return;
		}
	}

	auto newS(s);
	newS.push_back(cond);
	if (!tryHandleTarget(newS, objPath) && verbose)
		std::cerr << objPath << " source not found\n";
}

void TreeWalker::handleKbuildFile(const CondStack &s, const std::filesystem::path &kbPath)
{
	/*print(colored('%*s%s/%s' % (len(path.parts) * 2, "", path, kb_file), 'green'));
	prefix = '%*s' % (len(kb_path.parts) * 2, "");*/
	if (verbose > 1)
		std::cout << __func__ << ": " << kbPath << "\n";

	parser.parse(archs, kbPath.string(), [this, &kbPath, &s](const std::string &cond,
		     const MP::MakeExprListener::EntryType &type,
		     const std::string &entry) {
		if (type == MP::MakeExprListener::Directory) {
			auto dir = kbPath.parent_path() / entry;
			if (!visited.insert(dir).second)
				return;
			if (verbose > 1)
				std::cout << "pushing dir: " << dir << "\n";
			auto newS(s);
			newS.push_back(cond);
			addDirectory(newS, dir);
		} else if (type == MP::MakeExprListener::Object) {
			auto newS(s);
			newS.push_back(cond);
			handleObject(newS, kbPath.parent_path() / entry);
		}
	});
}

void TreeWalker::addDirectory(const CondStack &s, const std::filesystem::path &path)
{
	if (verbose > 1) {
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

	std::cerr << __func__ << ": Kbuild/Makefile not found in " << path << "\n";
}

void TreeWalker::walk()
{
	while (!toWalk.empty()) {
		auto top = toWalk.back();
		toWalk.pop_back();
		handleKbuildFile(top.first, top.second);
	}
}
