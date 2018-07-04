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
#include "control.h"
#include "sharedActionSelectorCheck.h"

namespace LLBMV2 {


bool ControlConverter::preorder(const IR::P4Action* t){
    if(backend->action_function.count(t->controlPlaneName()) == 0) {
        Function *old_func = backend->function;
        BasicBlock *old_bb = backend->Builder.GetInsertBlock();

        std::vector<Type*> args;
        std::vector<std::string> names;
        std::set<std::string> allnames;
        std::map<cstring, std::vector<llvm::Value *> > action_call_args;    //append these args at end
        for (auto param : *(t->parameters)->getEnumerator()) {
            //add this parameter type to args
            if(param->hasOut())
                args.push_back(PointerType::get(backend->getType(param->type), 0));
            else
                args.push_back(backend->getType(param->type));
            names.push_back("alloca_"+std::string(param->name.name));
            allnames.insert(std::string(param->name.name));
        }
        

        action_call_args[t->name.name] = std::vector<llvm::Value *>(0);
        //add more parameters from outer scopes
        auto &vars = backend->st.getVars(backend->st.getCurrentScope());
        for (auto vp : vars) {
            if (allnames.find(std::string(vp.first)) == allnames.end()) {
                args.push_back(vp.second->getType());
                action_call_args[t->name.name].push_back(vp.second);
                names.push_back(std::string(vp.first));
                allnames.insert(std::string(vp.first));
            }
        }
        backend->st.enterScope();

        FunctionType *FT = FunctionType::get(Type::getVoidTy(backend->TheContext), args, false);
        backend->function = Function::Create(FT, Function::ExternalLinkage, Twine(t->controlPlaneName()), backend->TheModule.get());
        backend->function->setAttributes(backend->function->getAttributes().addAttribute(backend->TheContext, AttributeList::FunctionIndex, "action"));
        assert(backend->function->getAttributes().hasAttributes(AttributeList::FunctionIndex) && "attribute not set");
        backend->action_function[t->controlPlaneName()] = backend->function;
        backend->bbInsert = BasicBlock::Create(backend->TheContext, "entry", backend->function);

        backend->Builder.SetInsertPoint(backend->bbInsert);

        auto names_iter = names.begin();
        for (auto arg = backend->function->arg_begin(); arg != backend->function->arg_end(); arg++)
        {
            //name the argument
            arg->setName(Twine(*names_iter));
            if(!arg->getType()->isPointerTy()) {
                AllocaInst *alloca = backend->Builder.CreateAlloca(arg->getType());
                backend->Builder.CreateStore(arg, alloca);
                backend->st.insert(std::string(*names_iter),alloca);
            }
            else
                backend->st.insert(std::string(*names_iter),arg);            
            names_iter++;
        }

        t->body->apply(*toIR);

        backend->st.exitScope();
        backend->function = old_func;   
        backend->bbInsert = old_bb; 
        backend->Builder.CreateRetVoid();

        backend->Builder.SetInsertPoint(backend->bbInsert);
    }
    return false;
}

/**
    Custom visitor to enable traversal on other blocks
*/
bool ControlConverter::preorder(const IR::PackageBlock *block) {
    for (auto it : block->constantValue) {
        if (it.second->is<IR::ControlBlock>()) {
            visit(it.second->getNode());
        }
    }
    return false;
}

bool ControlConverter::preorder(const IR::ControlBlock* block) {
    auto bt = backend->pipeline_controls.find(block->container->name);
    if (bt == backend->pipeline_controls.end()) {
        return false;
    }

    const IR::P4Control* cont = block->container;

    auto it = backend->pipeline_namemap.find(block->container->name);
    BUG_CHECK(it != backend->pipeline_namemap.end(),
              "Expected to find %1% in control block name map", block->container->name);

    backend->st.enterScope();
    auto pl = cont->type->getApplyParameters();
    std::vector<Type*> control_function_args; 
    for (auto p : pl->parameters)
        control_function_args.push_back(PointerType::get(backend->getType(p->type), 0)); // push type of parameter
    
    FunctionType *control_function_type = FunctionType::get(Type::getInt32Ty(backend->TheContext), control_function_args, false);
    Function *control_function = Function::Create(control_function_type, Function::ExternalLinkage,  std::string(cont->getName().toString()), backend->TheModule.get());
    backend->function = control_function;

    control_function->setAttributes(control_function->getAttributes().addAttribute(backend->TheContext, 
                                        AttributeList::FunctionIndex, backend->pipeline_namemap[block->container->name].c_str()));
    assert(control_function->getAttributes().hasAttributes(AttributeList::FunctionIndex) && "attribute not set");

    Function::arg_iterator args = control_function->arg_begin();

    BasicBlock *init_block = BasicBlock::Create(backend->TheContext, "entry", control_function);
    backend->Builder.SetInsertPoint(init_block);
    for (auto p : pl->parameters){
        args->setName(std::string(p->name.name));
        backend->st.insert("alloca_"+std::string(p->name.name),args);
        args++;
    }

    for (auto c : cont->controlLocals) {
        if(auto s = c->to<IR::Declaration_Variable>())  {
            s->apply(*toIR);    
            continue;
        }
        if(auto s = c->to<IR::P4Action>()) {
            visit(s);
            continue;
        }
        if (c->is<IR::Declaration_Constant>() ||
            c->is<IR::P4Table>()){
            continue;}
        if (c->is<IR::Declaration_Instance>()) {
            auto bl = block->getValue(c);
            CHECK_NULL(bl);
            if (bl->is<IR::ControlBlock>() || bl->is<IR::ParserBlock>())
                // Since this block has not been inlined, it is probably unused
                // So we don't do anything.
                continue;
            if (bl->is<IR::ExternBlock>()) {
                // auto eb = bl->to<IR::ExternBlock>();
                // backend->getSimpleSwitch()->convertExternInstances(c, eb, action_profiles,
                //                                                    selector_check);
                continue;
            }
        }
        P4C_UNIMPLEMENTED("%1%: not yet handled", c);
    }

    cont->body->apply(*toIR);

    backend->Builder.CreateRet(ConstantInt::get(Type::getInt32Ty(backend->TheContext), 1));  
    return false;
}

void ChecksumConverter::convertChecksum(const IR::BlockStatement *block, bool verify) {
    auto typeMap = backend->getTypeMap();
    auto refMap = backend->getRefMap();
    for (auto stat : block->components) {
        if (auto blk = stat->to<IR::BlockStatement>()) {
            convertChecksum(blk, verify);
            continue;
        } 
        else if (auto mc = stat->to<IR::MethodCallStatement>()) {
            auto mi = P4::MethodInstance::resolve(mc, refMap, typeMap);
            if (auto em = mi->to<P4::ExternFunction>()) {
                cstring functionName = em->method->name.name;
                if ((verify && (functionName == v1model.verify_checksum.name ||
                                functionName == v1model.verify_checksum_with_payload.name)) ||
                    (!verify && (functionName == v1model.update_checksum.name ||
                                 functionName == v1model.update_checksum_with_payload.name))) {

                    BUG_CHECK(mi->expr->arguments->size() == 4, "%1%: Expected 4 arguments", mi);

                    auto ei = P4::EnumInstance::resolve(mi->expr->arguments->at(3), typeMap);
                    if (ei->name != "csum16") {
                        ::error("%1%: the only supported algorithm is csum16",
                                mi->expr->arguments->at(3));
                        return;
                    }
                    toIR->createExternFunction(3,mi->expr,functionName,mi);
                    continue;
                }
            }
        }
        ::error("%1%: Only calls to %2% or %3% allowed", stat,
                verify ? v1model.verify_checksum.name : v1model.update_checksum.name,
                verify ? v1model.verify_checksum_with_payload.name :
                v1model.update_checksum_with_payload.name);
    }
}


bool ChecksumConverter::preorder(const IR::PackageBlock *block) {
    for (auto it : block->constantValue) {
        if (it.second->is<IR::ControlBlock>()) {
            visit(it.second->getNode());
        }
    }
    return false;
}

bool ChecksumConverter::preorder(const IR::ControlBlock* block) {
    auto it = backend->update_checksum_controls.find(block->container->name);
    Function *control_function;
    if (backend->update_checksum_controls.find(block->container->name) != backend->update_checksum_controls.end() ||
        backend->verify_checksum_controls.find(block->container->name) !=  backend->verify_checksum_controls.end()) {
        if (backend->target == Target::SIMPLE) {
            backend->st.enterScope();
            auto pl = block->container->type->getApplyParameters();
            std::vector<Type*> control_function_args; 
            for (auto p : pl->parameters)
                control_function_args.push_back(PointerType::get(backend->getType(p->type),0)); // push type of parameter
            
            FunctionType *control_function_type = FunctionType::get(Type::getInt32Ty(backend->TheContext), control_function_args, false);
            control_function = Function::Create(control_function_type, Function::ExternalLinkage,  std::string(block->container->name.toString()), backend->TheModule.get());
            backend->function = control_function;
            Function::arg_iterator args = control_function->arg_begin();

            BasicBlock *init_block = BasicBlock::Create(backend->TheContext, "entry", control_function);
            backend->Builder.SetInsertPoint(init_block);
            for (auto p : pl->parameters){
                args->setName(std::string(p->name.name));
                backend->st.insert("alloca_"+std::string(p->name.name),args);
                args++;
            }
        }        
    }
    if (it != backend->update_checksum_controls.end()) {
        if (backend->target == Target::SIMPLE) {
            convertChecksum(block->container->body,false);
            backend->Builder.CreateRet(ConstantInt::get(Type::getInt32Ty(backend->TheContext), 1));   
            control_function->setAttributes(control_function->getAttributes().addAttribute(backend->TheContext, AttributeList::FunctionIndex, "update_checksum"));
            assert(control_function->getAttributes().hasAttributes(AttributeList::FunctionIndex) && "attribute not set");   
        }
    } else {
        it = backend->verify_checksum_controls.find(block->container->name);
        if (it != backend->verify_checksum_controls.end()) {
            if (backend->target == Target::SIMPLE) {
                convertChecksum(block->container->body, true);
                backend->Builder.CreateRet(ConstantInt::get(Type::getInt32Ty(backend->TheContext), 1)); 
                control_function->setAttributes(control_function->getAttributes().addAttribute(backend->TheContext, AttributeList::FunctionIndex, "verify_checksum"));
                assert(control_function->getAttributes().hasAttributes(AttributeList::FunctionIndex) && "attribute not set");     
            }
        }
    }
    return false;
}

}  // namespace LLBMV2



