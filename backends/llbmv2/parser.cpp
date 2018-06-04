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

#include "parser.h"


namespace LLBMV2 {

bool ParserConverter::preorder(const IR::ParserState* parserState) {
    // set this block as insert point
    backend->Builder.SetInsertPoint(backend->defined_state[parserState->name.name]);
    MYDEBUG(std::cout<< "SetInsertPoint = " << parserState->name.name;)
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
    //right now, only supports single key based select. and cases are allowed only on integer constant expressions
    /*
    if (defined_state.find("start") == defined_state.end()) {
        defined_state["start"] = BasicBlock::Create(TheContext, "start", function);
    }
    */
    if (backend->defined_state.find("reject") == backend->defined_state.end()) {
        backend->defined_state["reject"] = BasicBlock::Create(backend->TheContext, "reject", backend->function);
    }

    SwitchInst *sw = backend->Builder.CreateSwitch(toIR->processExpression(t->select->components[0],NULL,NULL), 
                                                        backend->defined_state["reject"], t->selectCases.size());
    std::cout<<"coming here1******************\n";
                                                        
    //comment above line and uncomment below commented code to test selectexpression with dummy select key
    
    // Value* tmp = ConstantInt::get(IntegerType::get(TheContext, 64), 1024, true);
    // SwitchInst *sw = Builder.CreateSwitch(tmp, defined_state["reject"], t->selectCases.size());
    

    bool issetdefault = false;
    for (unsigned i=0;i<t->selectCases.size();i++) {
        //visit(t->selectCases[i]);
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
            std::cout<<"keyset = "<<*t->selectCases[i]->keyset<<"\ncoming here2******************\n";
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
    MYDEBUG(std::cout<< "SetInsertPoint = Parser Entry\n";)
    for (auto s : parser->parserLocals) {
        if(auto dv = s->to<IR::Declaration_Variable>())
            dv->apply(*toIR);            
    }
    for (auto p : pl->parameters) {
        if(p->type->toString() == "packet_in")
            continue;
        std::cout << "inserting " << p->name.name << " into st\n";
        args->setName(std::string(p->name.name));
        // AllocaInst *alloca = backend->Builder.CreateAlloca(args->getType());
        backend->st.insert("alloca_"+std::string(p->name.name),args);
        // backend->Builder.CreateStore(args, alloca);       
        args++;
    }
    std::cout << "here3\n";
    for (auto s : parser->states) {
        llvm::BasicBlock* bbInsert = llvm::BasicBlock::Create(backend->TheContext, std::string(s->name.name), parser_function);
        backend->defined_state[s->name.name] = bbInsert;
    }
    backend->Builder.CreateBr(backend->defined_state["start"]);
    for (auto s : parser->states) {
        MYDEBUG(std::cout << "Visiting State = " <<  s << std::endl;);
        visit(s);
    }
    // TypeParameters
    //  May Not Work
    // if (!parser->getTypeParameters()->empty())
    // {
    //     for (auto a : parser->getTypeParameters()->parameters)
    //     {
    //         // visits Type_Var
    //         // a->getVarName()
    //         // a->getDeclId()
    //         // a->getP4Type()
    //         // return(getCorrespondingType(t->getP4Type()));
    //         AllocaInst *alloca = backend->Builder.CreateAlloca(backend->getCorrespondingType(parser->getP4Type()));
    //         backend->st.insert("alloca_"+a->getVarName(),alloca);
    //     }
    // }
    // visit(t->annotations);
    // visit(t->getTypeParameters());    // visits TypeParameters

    // for (auto p : pl->parameters) {
    //     auto type = typeMap->getType(p);
    //     bool initialized = (p->direction == IR::Direction::In || p->direction == IR::Direction::InOut);
    //     // auto value = factory->create(type, !initialized); // Create type declaration
    // }
    // if (t->constructorParams->size() != 0) 
    //     visit(t->constructorParams); //visits Vector -> ParameterList
    // visit(&t->parserLocals); // visits Vector -> Declaration 
    // //Declare basis block for each state
    // for (auto s : t->states)  
    // {
    //     llvm::BasicBlock* bbInsert = llvm::BasicBlock::Create(TheContext, std::string(s->name.name), parser_function);
    //     defined_state[s->name.name] = bbInsert;
    // }
    // // visit all states
    // visit(&t->states);
    // for (auto s : t->states)
    // {
    //     MYDEBUG(std::cout << "Visiting State = " <<  s << std::endl;);
    //     visit(s);
    // }
    // Builder.SetInsertPoint(defined_state["accept"]); // on exit set entry of new inst in accept block
    // MYDEBUG(std::cout<< "SetInsertPoint = Parser -> Accept\n";)
    std::cout << "parser done\n";
    // convert parse state



    // for (auto state : parser->states) {
    //     if (state->name == IR::ParserState::reject || state->name == IR::ParserState::accept)
    //         continue;
    //     std::cout << "The current state is -- " << state << "\n";
    //     auto state_id = json->add_parser_state(parser_id, state->controlPlaneName());
    //     // convert statements
    //     for (auto s : state->components) {
    //         std::cout << "The current component is -- " << s << "\n";            
    //         convertParserStatement(s);
    //     }
        // convert transitions
        // if (state->selectExpression != nullptr) {
        //     if (state->selectExpression->is<IR::SelectExpression>()) {
        //         auto expr = state->selectExpression->to<IR::SelectExpression>();
        //         auto transitions = convertSelectExpression(expr);
        //         for (auto transition : transitions) {
        //             json->add_parser_transition(state_id, transition);
        //         }
        //         auto key = convertSelectKey(expr);
        //         json->add_parser_transition_key(state_id, key);
        //     } else if (state->selectExpression->is<IR::PathExpression>()) {
        //         auto expr = state->selectExpression->to<IR::PathExpression>();
        //         auto transition = convertPathExpression(expr);
        //         json->add_parser_transition(state_id, transition);
        //     } else {
        //         BUG("%1%: unexpected selectExpression", state->selectExpression);
        //     }
        // } else {
        //     auto transition = createDefaultTransition();
        //     json->add_parser_transition(state_id, transition);
        // }
    // }
    backend->st.exitScope();
    std::cout<<"returning from preorder of parser\n";
    return false;
}

/// visit ParserBlock only
bool ParserConverter::preorder(const IR::PackageBlock* block) {
    for (auto it : block->constantValue) {
        if (it.second->is<IR::ParserBlock>()) {
            std::cout << "current packageblock -- " << it.second <<"\n";
            visit(it.second->getNode());
        }
    }
    return false;
}

}  // namespace LLBMV2
