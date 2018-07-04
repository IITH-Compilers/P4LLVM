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

#ifndef _BACKENDS_BMV2_BACKEND_H_
#define _BACKENDS_BMV2_BACKEND_H_

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
#include "metermap.h"
#include "midend/convertEnums.h"
#include "options.h"
#include "portableSwitch.h"
#include "simpleSwitch.h"

#include "llvmIRHeaders.h"
#include "scopeTable.h"
#include <dlfcn.h>
#include "json-backend/llvm-json.h"
#include "json-backend/redundantStoreElimination.h"


namespace LLBMV2 {

class Backend : public PassManager {
    using DirectCounterMap = std::map<cstring, const IR::P4Table*>;

    // TODO(hanw): current implementation uses refMap and typeMap from midend.
    // Once all midend passes are refactored to avoid patching refMap, typeMap,
    // We can regenerated the refMap and typeMap in backend.
    P4::ReferenceMap*                refMap;
    P4::TypeMap*                     typeMap;
    P4::ConvertEnums::EnumMapping*   enumMap;
    const IR::ToplevelBlock*         toplevel;
    P4::P4CoreLibrary&               corelib;
    ProgramParts                     structure;
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
    Target                           target;
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

 public:
    void process(const IR::ToplevelBlock* block, BMV2Options& options);
    void convert();
    P4::P4CoreLibrary &   getCoreLibrary() const   { return corelib; }
    ErrorCodesMap &       getErrorCodesMap()       { return errorCodesMap; }
    // P4::ConvertEnums::EnumMapping* getEnumMap() {return enumMap;}
    DirectCounterMap &    getDirectCounterMap()    { return directCounterMap; }
    DirectMeterMap &      getMeterMap()  { return meterMap; }
    P4::PortableModel &   getModel()     { return model; }
    ProgramParts &        getStructure() { return structure; }
    P4::ReferenceMap*     getRefMap()    { return refMap; }
    P4::TypeMap*          getTypeMap()   { return typeMap; }
    P4V1::SimpleSwitch*   getSimpleSwitch()        { return simpleSwitch; }
    const IR::ToplevelBlock* getToplevelBlock() { CHECK_NULL(toplevel); return toplevel; }

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
    std::map<cstring, std::vector<cstring> > enums;
    
    std::map<llvm::Type *, std::map<std::string, int> > structIndexMap;
    std::map<cstring, llvm::Type *> defined_type;
    std::map<cstring, llvm::BasicBlock *> defined_state;
    std::map<cstring, std::vector<cstring> > action_list_enum;

    llvm::Type* getCorrespondingType(const IR::Type *t);
    llvm::Value* processExpression(const IR::Expression *e, BasicBlock* bbIf=nullptr, BasicBlock* bbElse=nullptr, bool required_alloca=false);
    
    SmallVector<Metadata*, 4> headerMDV, structMDV, huMDV, errorsMDV;
    
    Backend(bool isV1, P4::ReferenceMap* refMap, P4::TypeMap* typeMap,
            P4::ConvertEnums::EnumMapping* enumMap, cstring fileName) :
        refMap(refMap), typeMap(typeMap), enumMap(enumMap),
        corelib(P4::P4CoreLibrary::instance),
        model(P4::PortableModel::instance),
        simpleSwitch(new P4V1::SimpleSwitch(this)),
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
            
            Builder.SetInsertPoint(bbInsert);
            Builder.CreateRetVoid();
    }

    std::map<cstring, llvm::Function *> action_function;    
    std::map<std::string, Value*> str;

    void dumpLLVMIR() {
        std::error_code ec;         
        S = new raw_fd_ostream(fileName+".ll", ec, sys::fs::F_RW);
        assert(!TheModule->empty() && "module is empty");
        TheModule->print(*S,nullptr);
        // TheModule->dump();
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

        for(auto e : enums) {
            SmallVector<Metadata*, 8> enumMDV;
            for(auto inner : e.second)  {
                enumMDV.push_back(MDString::get(TheContext, inner.c_str()));
            }
            MDNode* enumsMD = MDNode::get(TheContext, enumMDV);        
            NamedMDNode *enumsNMD = TheModule->getOrInsertNamedMetadata(e.first.c_str());
            enumsNMD->addOperand(enumsMD);
        }
    }

    Type* getType(const IR::Type *t) {
        assert(t != nullptr && "Type cannot be empty");    
        Type* type = getCorrespondingType(t);
        if(type->isVectorTy()) {
            auto seqtype = dyn_cast<SequentialType>(type);
            auto width = seqtype->getNumElements();
            return Type::getIntNTy(TheContext, width);
        }
        return type;
    }
    void runLLVMPasses(BMV2Options &options)
    {
        legacy::PassManager MPM;
        if(options.optimize) {
            std::unique_ptr<legacy::FunctionPassManager> FPM;
            FPM.reset(new legacy::FunctionPassManager(TheModule.get()));
            PassManagerBuilder PMBuilder;
            PMBuilder.OptLevel = 2;
            PMBuilder.SizeLevel = 2;
            PMBuilder.populateFunctionPassManager(*FPM);
            PMBuilder.populateModulePassManager(MPM);
            MPM.add(createAggressiveDCEPass());
            FPM->add(createStoreEliminationPass());
            FPM->doInitialization();
            for (Function &F : *TheModule)
                FPM->run(F);
            FPM->doFinalization();
        }
        MPM.add(createJsonBackendPass(options.outputFile));
        MPM.run(*TheModule.get());
        return;
    }
};

}  // namespace LLBMV2

#endif /* _BACKENDS_BMV2_BACKEND_H_ */
