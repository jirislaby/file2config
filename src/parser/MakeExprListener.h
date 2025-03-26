#ifndef MAKEEXPRLISTENER_H
#define MAKEEXPRLISTENER_H

#include <functional>
#include <string>

#include "MakeBaseListener.h"

namespace MP {

class MakeExprListener : public MakeBaseListener {
public:
        enum EntryType {
                Directory,
                Object,
        };
        using Callback = std::function<void(const std::string &cond, const enum EntryType &,
                const std::string &)>;

        MakeExprListener() = delete;
        MakeExprListener(const std::vector<std::string> &archs, const Callback &CB,
                         const std::string &lookingFor = "obj-")
                : MakeBaseListener(), archs(archs), CB(CB), lookingFor(lookingFor) {}

        virtual void exitExprAssign(MakeParser::ExprAssignContext *) override;
private:
        std::vector<std::string> evaluateAtom(MakeParser::AtomContext *atom);
        void evaluateWord(const std::string &cond, const MakeParser::WordContext *word);

        const std::vector<std::string> &archs;
        const Callback &CB;
        std::string lookingFor;
};

}

#endif
