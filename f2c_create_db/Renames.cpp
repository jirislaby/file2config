// SPDX-License-Identifier: GPL-2.0-only

#include <sl/helpers/Color.h>
#include <sl/helpers/Exception.h>
#include <sl/helpers/Process.h>
#include <sl/helpers/PtrStore.h>
#include <sl/helpers/String.h>

#include "Renames.h"

using Clr = SlHelpers::Color;
using RunEx = SlHelpers::RuntimeException;
using SlHelpers::raise;

using namespace F2C;

void Renames::processRenamesBetween(SQL::F2CSQLConn &sql, const SlGit::Repo &lrepo,
				    const BranchProps &begin, std::string_view end,
				    RenameMap &renames)
{
	auto begVersion = begin.version;
	std::ostringstream range;
	range << 'v' << begin.versionStr << "..";
	if (end.empty())
		range << "origin/master";
	else
		range << 'v' << end;

	Clr() << '\t' << range.str();

	// libgit2 is *very* slow at comparing trees, we have to call git log.
	SlHelpers::Process p;
	p.spawn("/usr/bin/git", { "-C", lrepo.workDir(), "log", "-M30", "-l0", "--oneline",
				  "--no-merges", "--raw", "--diff-filter=R",
				  "--format=", range.str() }, true);

	SlHelpers::PtrStore<FILE, decltype([](FILE *f) { if (f) fclose(f); })> stream;
	stream.reset(fdopen(p.readPipe(), "r"));
	if (!stream)
		RunEx("Cannot open stdout of git").raise();

	SlHelpers::PtrStore<char, decltype([](char *ptr) { free(ptr); })> lineRaw;
	size_t len = 0;

	while (getline(lineRaw.ptr(), &len, stream.get()) != -1) {
		auto line = lineRaw.str();
		if (line.empty() || line.front() != ':')
			RunEx("Bad line: ") << line << raise;

		auto vec = SlHelpers::String::splitSV(line, " \t\n");
		if (vec.size() < 7)
			RunEx("Bad formatted line: ") << line << raise;

		unsigned int similarity{};
		std::from_chars(vec[4].data() + 1, vec[4].data() + vec[4].size(), similarity);
		if (!similarity)
			RunEx("Bad rename part: ") << std::string(vec[4]) << raise;
		auto oldFile = vec[5];
		auto newFile = vec[6];

		auto it = renames.find(newFile);
		if (it != renames.end()) {
			auto final = std::move(it->second);
			renames.erase(it);

			// do not store reverted and back and forth renames
			if (oldFile != final.path) {
				final.similarity *= similarity;
				final.similarity /= 100U;
				renames.emplace(oldFile, std::move(final));
			}
		} else {
			renames.emplace(oldFile, RenameInfo{std::string(newFile), similarity});
		}
	}

	if (!feof(stream.get()) || ferror(stream.get()))
		RunEx("Not completely read: ") << strerror(errno) << raise;

	if (!p.waitForFinished())
		RunEx("Cannot wait for git: ") << p.lastError() << raise;

	if (p.signalled())
		RunEx("git crashed").raise();
	if (auto e = p.exitStatus())
		RunEx("git exited with ") << e << raise;

	auto trans = sql.beginAuto();
	for (const auto &e: renames) {
		auto oldP = sql.insertPath(e.first);
		if (!oldP)
			RunEx("Cannot insert old path: ") << e.first << ": " <<
						 sql.lastError() << raise;
		auto newP = sql.insertPath(e.second.path);
		if (!newP)
			RunEx("Cannot insert new path: ") << e.second.path << ": " <<
						 sql.lastError() << raise;
		if (!sql.insertRFVMap(begVersion, e.second.similarity, oldP->first, oldP->second,
				       newP->first, newP->second))
			RunEx("Cannot insert rename file map: ") << e.first << " -> " <<
								    e.second.path << ": " <<
								    sql.lastError() << raise;
	}
}

void Renames::processRenames(SQL::F2CSQLConn &sql, const SlGit::Repo &lrepo,
			     const BranchesProps &branchesProps)
{
	auto uniqTags = getUniqTags(branchesProps);
	if (uniqTags.size() >= 1) {
		auto curr = uniqTags.rbegin();

		RenameMap map;
		processRenamesBetween(sql, lrepo, *curr, "", map);
		for (auto prev = std::next(curr); prev != uniqTags.rend(); ++curr, ++prev)
			processRenamesBetween(sql, lrepo, *prev, curr->versionStr, map);
	}
}

