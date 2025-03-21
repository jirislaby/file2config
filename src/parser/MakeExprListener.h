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
        MakeExprListener(const Callback &CB, const std::string &lookingFor = "obj-")
                : MakeBaseListener(), CB(CB), lookingFor(lookingFor) {}

        virtual void exitExprAssign(MakeParser::ExprAssignContext *) override;
private:
        const Callback &CB;
        std::string lookingFor;
};

}

#endif
