#ifndef CONSOLEMAKEVISITOR_H
#define CONSOLEMAKEVISITOR_H

#include <iostream>

#include "MakeVisitor.h"

namespace TW {

class ConsoleMakeVisitor : public MakeVisitor
{
public:
	ConsoleMakeVisitor() {}
	virtual ~ConsoleMakeVisitor() override {}
	virtual void ignored(const std::filesystem::path &objPath,
			     const std::string &cond) const override {
		std::cout << "ignoring already reported " << objPath << ", now with " << cond << '\n';
	}

	virtual void config(const std::filesystem::path &srcPath,
			    const std::string &cond) const override {
		std::cout << "XXX " << cond << " " << srcPath.string() << "\n";
	}
private:
};

}

#endif // CONSOLEMAKEVISITOR_H
