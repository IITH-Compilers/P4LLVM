/*
Copyright 2013-present Barefoot Networks, Inc.

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

#ifndef _BACKENDS_BMV2_BACKEND_H_
#define _BACKENDS_BMV2_BACKEND_H_

#include "analyzer.h"
#include "expression.h"
#include "frontends/common/model.h"
#include "frontends/p4/coreLibrary.h"
#include "helpers.h"
#include "ir/ir.h"
#include "lib/error.h"
#include "lib/exceptions.h"
#include "lib/gc.h"
#include "lib/json.h"
#include "lib/log.h"
#include "lib/nullstream.h"
#include "JsonObjects.h"
#include "metermap.h"
#include "midend/convertEnums.h"
#include "options.h"
#include "portableSwitch.h"
#include "simpleSwitch.h"

#include "llvmIRHeaders.h"
#include "scopeTable.h"
// #include "toIR.h"

namespace LLBMV2 {

class ExpressionConverter;

class Backend : public PassManager {
    using DirectCounterMap = std::map<cstring, const IR::P4Table*>;

    // TODO(hanw): current implementation uses refMap and typeMap from midend.
    // Once all midend passes are refactored to avoid patching refMap, typeMap,
    // We can regenerated the refMap and typeMap in backend.
    P4::ReferenceMap*                refMap;
    P4::TypeMap*                     typeMap;
    P4::ConvertEnums::EnumMapping*   enumMap;
    const IR::ToplevelBlock*         toplevel;
    ExpressionConverter*             conv;
    P4::P4CoreLibrary&               corelib;
    ProgramParts                     structure;
    Util::JsonObject                 jsonTop;
    P4::PortableModel&               model;  // remove
    DirectCounterMap                 directCounterMap;
    DirectMeterMap                   meterMap;
    ErrorCodesMap                    errorCodesMap;

    // bmv2 backend supports multiple target architectures, we create different
    // json generators for each architecture to handle the differences in json
    // format for each architecture.
    P4V1::SimpleSwitch*              simpleSwitch;
    // PortableSwitchJsonConverter*  portableSwitch;

 public:
    LLBMV2::JsonObjects*               json;
    Target                           target;
    Util::JsonArray*                 counters;
    Util::JsonArray*                 externs;
    Util::JsonArray*                 field_lists;
    Util::JsonArray*                 learn_lists;
    Util::JsonArray*                 meter_arrays;
    Util::JsonArray*                 register_arrays;
    Util::JsonArray*                 force_arith;
    Util::JsonArray*                 field_aliases;

    // We place scalar user metadata fields (i.e., bit<>, bool)
    // in the scalarsName metadata object, so we may need to rename
    // these fields.  This map holds the new names.
    std::map<const IR::StructField*, cstring> scalarMetadataFields;

    std::set<cstring>                pipeline_controls;
    std::set<cstring>                non_pipeline_controls;
    std::set<cstring>                update_checksum_controls;
    std::set<cstring>                verify_checksum_controls;
    std::set<cstring>                deparser_controls;

    // bmv2 expects 'ingress' and 'egress' pipeline to have fixed name.
    // provide an map from user program block name to hard-coded names.
    std::map<cstring, cstring>       pipeline_namemap;

 protected:
    ErrorValue retrieveErrorValue(const IR::Member* mem) const;
    void createFieldAliases(const char *remapFile);
    void genExternMethod(Util::JsonArray* result, P4::ExternMethod *em);

 public:
    void process(const IR::ToplevelBlock* block, BMV2Options& options);
    void convert(BMV2Options& options);
    void serialize(std::ostream& out) const
    { jsonTop.serialize(out); }
    P4::P4CoreLibrary &   getCoreLibrary() const   { return corelib; }
    ErrorCodesMap &       getErrorCodesMap()       { return errorCodesMap; }
    ExpressionConverter * getExpressionConverter() { return conv; }
    DirectCounterMap &    getDirectCounterMap()    { return directCounterMap; }
    DirectMeterMap &      getMeterMap()  { return meterMap; }
    P4::PortableModel &   getModel()     { return model; }
    ProgramParts &        getStructure() { return structure; }
    P4::ReferenceMap*     getRefMap()    { return refMap; }
    P4::TypeMap*          getTypeMap()   { return typeMap; }
    P4V1::SimpleSwitch*   getSimpleSwitch()        { return simpleSwitch; }
    const IR::ToplevelBlock* getToplevelBlock() { CHECK_NULL(toplevel); return toplevel; }
    /// True if this parameter represents the standard_metadata input.
    bool isStandardMetadataParameter(const IR::Parameter* param);

    //Code for emitting LLVM-IR
public:
    llvm::IRBuilder<> Builder;
    BasicBlock *bbInsert;
    Function *function;
    LLVMContext TheContext;
    std::unique_ptr<Module> TheModule;
    raw_fd_ostream *S;
    ScopeTable<Value*> st;
    cstring fileName;
    // ToIR* toIR;

    std::map<cstring, std::vector<llvm::Value *> > action_call_args;    //append these args at end
    std::map<cstring,  std::vector<bool> > action_call_args_isout;
    std::map<llvm::Type *, std::map<std::string, int> > structIndexMap;
    std::map<cstring, llvm::Type *> defined_type;
    std::map<cstring, llvm::BasicBlock *> defined_state;
    std::map<cstring, std::vector<cstring> > action_list_enum;

    llvm::Type* getCorrespondingType(const IR::Type *t);
    llvm::Value* processExpression(const IR::Expression *e, BasicBlock* bbIf=nullptr, BasicBlock* bbElse=nullptr, bool required_alloca=false);
    
    SmallVector<Metadata*, 4> headerMDV, structMDV, huMDV, errorsMDV;
    
    int i =0; //Debug info

    unsigned getByteAlignment(unsigned width) {
        if (width <= 8)
            return 8;
        else if (width <= 16)
            return 16;
        else if (width <= 32)
            return 32;
        else if (width <= 64)
            return 64;
        else
            // compiled as u8* 
            return 64;
    }
public:
    Backend(bool isV1, P4::ReferenceMap* refMap, P4::TypeMap* typeMap,
            P4::ConvertEnums::EnumMapping* enumMap, cstring fileName) :
        refMap(refMap), typeMap(typeMap), enumMap(enumMap),
        corelib(P4::P4CoreLibrary::instance),
        model(P4::PortableModel::instance),
        simpleSwitch(new P4V1::SimpleSwitch(this)),
        json(new LLBMV2::JsonObjects()),
        target(Target::SIMPLE),
        Builder(TheContext), 
        fileName(fileName) {
            refMap->setIsV1(isV1); 
            setName("BackEnd"); 
            TheModule = llvm::make_unique<Module>(fileName.c_str(), TheContext);
            std::vector<Type*> args;
            FunctionType *FT = FunctionType::get(Type::getVoidTy(TheContext), args, false);
            function = Function::Create(FT, Function::ExternalLinkage, "main", TheModule.get());
            
            bbInsert = BasicBlock::Create(TheContext, "entry", function);
            
            MYDEBUG(std::cout<< "SetInsertPoint = main\n";)
            Builder.SetInsertPoint(bbInsert);
            Builder.CreateRetVoid();
    }

    void dumpLLVMIR() {
        std::cout << "******************************************************************************\n\n";
        std::error_code ec;         
        S = new raw_fd_ostream(fileName+".ll", ec, sys::fs::F_RW);
        assert(!TheModule->empty() && "module is empty");
        TheModule->print(*S,nullptr);
        TheModule->dump();
    }

    void addMetaData() {
        MDNode* headerMD = MDNode::get(TheContext, headerMDV);
        NamedMDNode *headerNMD = TheModule->getOrInsertNamedMetadata("header");
        headerNMD->addOperand(headerMD);

        MDNode* structMD = MDNode::get(TheContext, structMDV);        
        NamedMDNode *structNMD = TheModule->getOrInsertNamedMetadata("struct");
        structNMD->addOperand(structMD);

        MDNode* huMD = MDNode::get(TheContext, huMDV);        
        NamedMDNode *huNMD = TheModule->getOrInsertNamedMetadata("header_union");
        huNMD->addOperand(huMD);  

        MDNode* errorsMD = MDNode::get(TheContext, errorsMDV);        
        NamedMDNode *errorsNMD = TheModule->getOrInsertNamedMetadata("errors");
        errorsNMD->addOperand(errorsMD);   
    }
};

}  // namespace LLBMV2

#endif /* _BACKENDS_BMV2_BACKEND_H_ */
