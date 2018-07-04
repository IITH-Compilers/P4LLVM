/*
IITH Compilers
authors: S Venkata Keerthy, D Tharun
email: {cs17mtech11018, cs15mtech11002}@iith.ac.in

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#ifndef _TOIR_H_
#define _TOIR_H_

#include "llvmIRHeaders.h"
#include "scopeTable.h"
#include "backend.h"
#include "frontends/p4/fromv1.0/v1model.h"
#include "frontends/p4/tableApply.h"


using namespace P4;
using namespace LLBMV2;

class ToIR : public Inspector {
    Backend* backend;
    TypeMap* typemap;
    ReferenceMap* refmap;
    P4::P4CoreLibrary&   corelib;
    Value* llvmValue;
    P4V1::V1Model& v1model;

    Value* createTableFunction(int no, const IR::ConstructorCallExpression* mce, cstring name);
    void convertTableEntries(const IR::P4Table *table, Instruction* apply);
    cstring getKeyMatchType(const IR::KeyElement *ke);
    Value* handleTableImplementation(const IR::Property* implementation);
    void convertTable(const IR::P4Table* table); 
    
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
    // bool preorder(const IR::SwitchStatement* t) override;
        
    
    Value* createExternFunction(int no, const IR::MethodCallExpression* mce, cstring name, MethodInstance *minst);
    llvm::Value* processExpression(const IR::Expression *e, BasicBlock* bbIf=nullptr, BasicBlock* bbElse=nullptr, bool required_alloca=false);    
};

#endif