#ifndef _TOIR_H_
#define _TOIR_H_

#include "llvmIRHeaders.h"
#include "scopeTable.h"
#include "backend.h"
#include "frontends/p4/fromv1.0/v1model.h"


using namespace P4;
using namespace LLBMV2;
// using namespace std;

class ToIR : public Inspector {
    Backend* backend;
    TypeMap* typemap;
    ReferenceMap* refmap;
    P4::P4CoreLibrary&   corelib;
    Value* llvmValue;
    P4V1::V1Model& v1model;
    
public:
    ToIR(Backend* backend) : backend(backend), corelib(P4::P4CoreLibrary::instance), v1model(P4V1::V1Model::instance) {
        setName("ToIR");
        typemap = backend->getTypeMap();
        refmap = backend->getRefMap();
    }

    bool preorder(const IR::Declaration_Variable* t) override;
    bool preorder(const IR::AssignmentStatement* t) override;
    bool preorder(const IR::IfStatement* t) override;
    bool preorder(const IR::BlockStatement* b) override;
    bool preorder(const IR::MethodCallStatement* stat) override;    
    
    void createExternFunction(int no, const IR::MethodCallExpression* mce, cstring name);
    llvm::Value* processExpression(const IR::Expression *e, BasicBlock* bbIf=nullptr, BasicBlock* bbElse=nullptr, bool required_alloca=false);    
};

#endif