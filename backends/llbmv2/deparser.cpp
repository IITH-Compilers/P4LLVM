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
#include "deparser.h"

namespace LLBMV2 {

void ConvertDeparser::convertDeparserBody(const IR::Vector<IR::StatOrDecl>* body) {
    for (auto s : *body) {
        if (auto block = s->to<IR::BlockStatement>()) {
            convertDeparserBody(&block->components);
            continue;
        } else if (s->is<IR::ReturnStatement>() || s->is<IR::ExitStatement>()) {
            break;
        } else if (s->is<IR::EmptyStatement>()) {
            continue;
        } else if (s->is<IR::MethodCallStatement>()) {
            auto mc = s->to<IR::MethodCallStatement>()->methodCall;
            auto mi = P4::MethodInstance::resolve(mc,
                    refMap, typeMap);
            if (mi->is<P4::ExternMethod>()) {
                auto em = mi->to<P4::ExternMethod>();
                if (em->originalExternType->name.name == backend->getCoreLibrary().packetOut.name) {
                    if (em->method->name.name == backend->getCoreLibrary().packetOut.emit.name) {
                        BUG_CHECK(mc->arguments->size() == 1,
                                  "Expected exactly 1 argument for %1%", mc);
                        toIR->createExternFunction(1, mc, em->method->name.name,mi);
                    }
                    continue;
                }
            }
        }
        ::error("%1%: not supported with a deparser on this target", s);
    }
}

void ConvertDeparser::convertDeparser(const IR::P4Control* cont) {
    backend->st.enterScope();
    auto pl = cont->type->getApplyParameters();
    std::vector<Type*> control_function_args; 
    for (auto p : pl->parameters) {
        if(p->type->toString() == "packet_out")
            continue;
        control_function_args.push_back(PointerType::get(backend->getCorrespondingType(p->type),0)); // push type of parameter
    }
    
    FunctionType *control_function_type = FunctionType::get(Type::getInt32Ty(backend->TheContext), control_function_args, false);
    Function *control_function = Function::Create(control_function_type, Function::ExternalLinkage,  std::string(cont->getName().toString()), backend->TheModule.get());
    backend->function = control_function;

    control_function->setAttributes(control_function->getAttributes().addAttribute(backend->TheContext, AttributeList::FunctionIndex, "Deparser"));
    assert(control_function->getAttributes().hasAttributes(AttributeList::FunctionIndex) && "attribute not set");

    Function::arg_iterator args = control_function->arg_begin();
    BasicBlock *init_block = BasicBlock::Create(backend->TheContext, "entry", control_function);
    backend->Builder.SetInsertPoint(init_block);
    for (auto p : pl->parameters){
        if(p->type->toString() == "packet_out")
            continue;
        args->setName(std::string(p->name.name));
        backend->st.insert("alloca_"+std::string(p->name.name),args);
        args++;
    }
    convertDeparserBody(&cont->body->components);
    backend->Builder.CreateRet(ConstantInt::get(Type::getInt32Ty(backend->TheContext), 1));
}

bool ConvertDeparser::preorder(const IR::PackageBlock* block) {
    for (auto it : block->constantValue) {
        if (it.second->is<IR::ControlBlock>()) {
            visit(it.second->getNode());
        }
    }
    return false;
}

bool ConvertDeparser::preorder(const IR::ControlBlock* block) {
    auto bt = backend->deparser_controls.find(block->container->name);
    if (bt == backend->deparser_controls.end()) {
        return false;
    }
    const IR::P4Control* cont = block->container;
    convertDeparser(cont);
    return false;
}

}  // namespace LLBMV2
