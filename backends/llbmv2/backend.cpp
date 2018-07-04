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

#include "backend.h"
#include "header.h"
#include "control.h"
#include "deparser.h"
#include "errorcode.h"
#include "frontends/common/constantFolding.h"
#include "frontends/p4/evaluator/evaluator.h"
#include "frontends/p4/methodInstance.h"
#include "frontends/p4/simplify.h"
#include "frontends/p4/typeChecking/typeChecker.h"
#include "frontends/p4/unusedDeclarations.h"
#include "midend/actionSynthesis.h"
#include "midend/removeLeftSlices.h"
#include "lower.h"
#include "header.h"
#include "parser.h"
#include "extractArchInfo.h"

namespace LLBMV2 {

/**
This class implements a policy suitable for the SynthesizeActions pass.
The policy is: do not synthesize actions for the controls whose names
are in the specified set.
For example, we expect that the code in the deparser will not use any
tables or actions.
*/
class SkipControls : public P4::ActionSynthesisPolicy {
    // set of controls where actions are not synthesized
    const std::set<cstring> *skip;

 public:
    explicit SkipControls(const std::set<cstring> *skip) : skip(skip) { CHECK_NULL(skip); }
    bool convert(const IR::P4Control* control) const {
        if (skip->find(control->name) != skip->end())
            return false;
        return true;
    }
};

/**
This class implements a policy suitable for the RemoveComplexExpression pass.
The policy is: only remove complex expression for the controls whose names
are in the specified set.
For example, we expect that the code in ingress and egress will have complex
expression removed.
*/
class ProcessControls : public LLBMV2::RemoveComplexExpressionsPolicy {
    const std::set<cstring> *process;

 public:
    explicit ProcessControls(const std::set<cstring> *process) : process(process) {
        CHECK_NULL(process);
    }
    bool convert(const IR::P4Control* control) const {
        if (process->find(control->name) != process->end())
            return true;
        return false;
    }
};

void
Backend::process(const IR::ToplevelBlock* tlb, BMV2Options& options) {
    CHECK_NULL(tlb);
    auto evaluator = new P4::EvaluatorPass(refMap, typeMap);
    if (tlb->getMain() == nullptr)
        return;  // no main

    if (options.arch != Target::UNKNOWN)
        target = options.arch;

    if (target == Target::SIMPLE) {
        simpleSwitch->setPipelineControls(tlb, &pipeline_controls, &pipeline_namemap);
        simpleSwitch->setNonPipelineControls(tlb, &non_pipeline_controls);
        simpleSwitch->setUpdateChecksumControls(tlb, &update_checksum_controls);
        simpleSwitch->setVerifyChecksumControls(tlb, &verify_checksum_controls);
        simpleSwitch->setDeparserControls(tlb, &deparser_controls);
    } else if (target == Target::PORTABLE) {
        P4C_UNIMPLEMENTED("PSA architecture is not yet implemented");
    }

    // These passes are logically bmv2-specific
    addPasses({
        new P4::SynthesizeActions(refMap, typeMap, new SkipControls(&non_pipeline_controls)),
        new P4::MoveActionsToTables(refMap, typeMap),
        new P4::TypeChecking(refMap, typeMap),
        new P4::SimplifyControlFlow(refMap, typeMap),
        new LowerExpressions(typeMap),
        new P4::ConstantFolding(refMap, typeMap, false),
        new P4::TypeChecking(refMap, typeMap),
        new RemoveComplexExpressions(refMap, typeMap, new ProcessControls(&pipeline_controls)),
        new P4::SimplifyControlFlow(refMap, typeMap),
        new P4::RemoveAllUnusedDeclarations(refMap),
        new DiscoverStructure(&structure),
        new ErrorCodesVisitor(&errorCodesMap, this),
        new ExtractArchInfo(typeMap),
        evaluator,
        new VisitFunctor([this, evaluator]() { toplevel = evaluator->getToplevelBlock(); }),
    });
    tlb->getProgram()->apply(*this);    
}

/// BMV2 Backend that takes the top level block and converts it to a JsonObject
/// that can be interpreted by the BMv2 simulator.
void Backend::convert() {
    PassManager programPasses = {
        new ConvertHeaders(this)
    };
    // if (psa) tlb->apply(new ConvertExterns());
    PassManager codegen_passes = {
        new ConvertParser(this),
        new ConvertControl(this),
        new ConvertDeparser(this),
    };

    codegen_passes.setName("CodeGen");
    CHECK_NULL(toplevel);
    auto main = toplevel->getMain();
    if (main == nullptr)
        return;

    toplevel->getProgram()->apply(programPasses);
    main->apply(codegen_passes);
}

llvm::Type* Backend::getCorrespondingType(const IR::Type *t) {
    assert(t != nullptr && "Type cannot be empty");
    if(t->is<IR::Type_Void>()) {
        return llvm::Type::getVoidTy(TheContext);
    }
    
    else if(t->is<IR::Type_Boolean>()) {
        llvm::Type *temp = Type::getInt1Ty(TheContext);                         
        defined_type[t->toString()] = temp;
        return temp;
    }
    
    // if int<> or bit<> /// bit<32> x; /// The type of x is Type_Bits(32); /// The type of 'bit<32>' is Type_Type(Type_Bits(32))
    else if(t->is<IR::Type_Bits>()) { // Bot int <> and bit<> passes this check
        const IR::Type_Bits *x =  dynamic_cast<const IR::Type_Bits *>(t);
        int width = x->width_bits();
        if(x->isSigned) //Int 
        {   
            llvm::Type *temp = Type::getIntNTy(TheContext, width);
            defined_type[t->toString()] = temp;
            return temp;
        }
        else //Bit
        {   
            llvm::Type *temp =  VectorType::get(Type::getInt1Ty(TheContext), width);                
            defined_type[t->toString()] = temp;
            return temp;
        }
    }
    
    else if(t->is<IR::Type_Varbits>())  {
        llvm::Type *temp = VectorType::get(Type::getInt1Ty(TheContext), t->to<IR::Type_Varbits>()->size);       
        defined_type[t->toString()] = temp;
        return temp;
    }

    // Derived Types
    else if(t->is<IR::Type_Name>()) {
        if(defined_type[t->toString()]) {
            return(defined_type[t->toString()]);
        }
        auto canon = typeMap->getTypeType(t, true);
        defined_type[t->toString()] = getCorrespondingType(canon);
        return defined_type[t->toString()];
    }
    
    else if (t->is<IR::Type_StructLike>()) {
        assert((t->is<IR::Type_Struct>() || t->is<IR::Type_Header>() || t->is<IR::Type_HeaderUnion>()) && "Unhandled Type_StructLike");
        if(defined_type[t->toString()]) { 
            llvm::Type *x = defined_type[t->toString()];
            // x->dump();
            llvm::StructType *y = dyn_cast<llvm::StructType>(x);
            
            if(!y->isOpaque()) // if opaque then define it
                return(x);
        }

        const IR::Type_StructLike *strct = dynamic_cast<const IR::Type_StructLike *>(t);

        // Vector to Store attributes(variables) of struct
        std::vector<Type*> members;
        for (auto f : strct->fields)
            members.push_back(getCorrespondingType(f->type));

        llvm::Type *temp =llvm::StructType::get(TheContext, members);
        defined_type[t->toString()] = temp;
        return temp;
    }

    //eg-> header Ethernet_t is refered by Ethernet_t (type name is Ethernet_t) 
    // c++ equal => typedef struct x x
    else if(t->is<IR::Type_Typedef>()) {
        const IR::Type_Typedef *x = dynamic_cast<const IR::Type_Typedef *>(t);
        return(getCorrespondingType(x->type)); 
    }

    else if(auto ty = t->to<IR::Type_Stack>()){
        if(auto st = ty->elementType->to<IR::Type_StructLike>()){
            if(defined_type[st->name]) {
                return ArrayType::get(defined_type[st->name], ty->getSize());
            } 
        }
        llvm_unreachable("stack should have been handled as structlike");
        return nullptr;
    }

    else if(t->is<IR::Type_Extern>()) {
        return nullptr;
    }

    else if (t->is<IR::Type_Error>()) {
        llvm::Type *temp = Type::getInt32Ty(TheContext);                         
        defined_type[t->toString()] = temp;
        return temp;    
    }

    llvm_unreachable("Unhandled type in getCorrespondingType()");
    return nullptr;
}

}  // namespace LLBMV2
