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

#include "parser.h"


namespace LLBMV2 {

bool ParserConverter::preorder(const IR::ParserState* parserState) {
    // set this block as insert point
    backend->Builder.SetInsertPoint(backend->defined_state[parserState->name.name]);
    if (parserState->name == "accept") {
        backend->Builder.CreateRet(ConstantInt::get(Type::getInt32Ty(backend->TheContext), 1));
        return false;

    }
    else if(parserState->name == "reject") {
        backend->Builder.CreateRet(ConstantInt::get(Type::getInt32Ty(backend->TheContext), 0));
        return false;
    }
    
    for (auto s : parserState->components) {
            s->apply(*toIR);                       
    }

    // if  select expression is null
    if (parserState->selectExpression == nullptr) // Create branch to reject(there should be some transition or select)
    {
        backend->Builder.CreateBr(backend->defined_state["reject"]); // if reject then return 0;
    } 
    // visit SelectExpression
    else if (parserState->selectExpression->is<IR::SelectExpression>()) {
        visit(parserState->selectExpression);
    } 
    //  Else transition expression
    else
    {
        // must be a PathExpression which is goto 'state' state name
        if (parserState->selectExpression->is<IR::PathExpression>())
        {
            const IR::PathExpression *x = dynamic_cast<const IR::PathExpression *>( parserState->selectExpression);
            backend->Builder.CreateBr(backend->defined_state[x->path->asString()]);
        }
        // bug 
        else
        {
            BUG("Got Unknown Expression type");            
        }
    }
    return false;
}

bool ParserConverter::preorder(const IR::SelectExpression* t) { 
    if (backend->defined_state.find("reject") == backend->defined_state.end()) {
        backend->defined_state["reject"] = BasicBlock::Create(backend->TheContext, "reject", backend->function);
    }

    SwitchInst *sw = backend->Builder.CreateSwitch(toIR->processExpression(t->select->components[0],NULL,NULL), 
                                                        backend->defined_state["reject"], t->selectCases.size());
      

    bool issetdefault = false;
    for (unsigned i=0;i<t->selectCases.size();i++) {
        if (dynamic_cast<const IR::DefaultExpression *>(t->selectCases[i]->keyset)) {
            if (issetdefault) continue;
            else {
                issetdefault = true;
                if (backend->defined_state.find(t->selectCases[i]->state->path->asString()) == backend->defined_state.end()) {
                    backend->defined_state[t->selectCases[i]->state->path->asString()] = BasicBlock::Create(backend->TheContext, 
                                                    Twine(t->selectCases[i]->state->path->asString()), backend->function);
                }
                sw->setDefaultDest(backend->defined_state[t->selectCases[i]->state->path->asString()]);
            }
        }
        else if (dynamic_cast<const IR::Constant *>(t->selectCases[i]->keyset)) {
            if (backend->defined_state.find(t->selectCases[i]->state->path->asString()) == backend->defined_state.end()) {
                backend->defined_state[t->selectCases[i]->state->path->asString()] = BasicBlock::Create(backend->TheContext, 
                                                     Twine(t->selectCases[i]->state->path->asString()), backend->function);
            }
            ConstantInt *onVal = (ConstantInt *) toIR->processExpression(t->selectCases[i]->keyset);
            sw->addCase(onVal, backend->defined_state[t->selectCases[i]->state->path->asString()]);
        }
    }
    return false;
}

bool ParserConverter::preorder(const IR::P4Parser* parser) {
    for (auto s : parser->parserLocals) {
        if (s->is<IR::Declaration_Instance>()) {
            ::error("%1%: not supported on parsers on this target", s);
            return false;
        }
    }

    auto pl = parser->type->getApplyParameters();
    backend->st.enterScope();
    std::vector<Type*> parser_function_args; 
    for (auto p : pl->parameters)
    {
        if(p->type->toString() == "packet_in")
            continue;
        parser_function_args.push_back(PointerType::get(backend->getCorrespondingType(p->type), 0));
    }
    
    FunctionType *parser_function_type = FunctionType::get(Type::getInt32Ty(backend->TheContext), parser_function_args, false);
    Function *parser_function = Function::Create(parser_function_type, Function::ExternalLinkage,  std::string(parser->getName().toString()), backend->TheModule.get());
    Function::arg_iterator args = parser_function->arg_begin();
    backend->function = parser_function;

    parser_function->setAttributes(parser_function->getAttributes().addAttribute(backend->TheContext, AttributeList::FunctionIndex, "parser"));
    assert(parser_function->getAttributes().hasAttributes(AttributeList::FunctionIndex) && "attribute not set");

    BasicBlock *init_block = BasicBlock::Create(backend->TheContext, "entry", parser_function);
    backend->Builder.SetInsertPoint(init_block);
    for (auto s : parser->parserLocals) {
        if(auto dv = s->to<IR::Declaration_Variable>())
            dv->apply(*toIR);            
    }
    for (auto p : pl->parameters) {
        if(p->type->toString() == "packet_in")
            continue;
        args->setName(std::string(p->name.name));
        backend->st.insert("alloca_"+std::string(p->name.name),args);
        args++;
    }
    for (auto s : parser->states) {
        llvm::BasicBlock* bbInsert = llvm::BasicBlock::Create(backend->TheContext, std::string(s->name.name), parser_function);
        backend->defined_state[s->name.name] = bbInsert;
    }
    backend->Builder.CreateBr(backend->defined_state["start"]);
    for (auto s : parser->states) {
        visit(s);
    }

    backend->st.exitScope();
    return false;
}

/// visit ParserBlock only
bool ParserConverter::preorder(const IR::PackageBlock* block) {
    for (auto it : block->constantValue) {
        if (it.second->is<IR::ParserBlock>()) {
            visit(it.second->getNode());
        }
    }
    return false;
}

}  // namespace LLBMV2
