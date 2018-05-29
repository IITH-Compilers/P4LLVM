#ifndef _TOIR_H_
#define _TOIR_H_

#include "llvmIRHeaders.h"
#include "scopeTable.h"
#include "backend.h"

using namespace P4;
using namespace LLBMV2;

class ToIR : public Inspector {
    Backend* backend;
    TypeMap* typemap;
    ReferenceMap* refmap;
    P4::P4CoreLibrary&   corelib;
    
public:
    ToIR(Backend* backend) : backend(backend), corelib(P4::P4CoreLibrary::instance) {
        setName("ToIR");
        typemap = backend->getTypeMap();
        refmap = backend->getRefMap();
    }

    llvm::Value* processExpression(const IR::Expression *e, BasicBlock* bbIf=nullptr, BasicBlock* bbElse=nullptr, bool required_alloca=false);    
};

#endif