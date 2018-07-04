/*
IITH Compilers
authors: D Tharun, S Venkata Keerthy
email: {cs15mtech11002, cs17mtech11018}@iith.ac.in

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

#include "emitParser.h"
#include "../JsonObjects.h"
#include "llvm/IR/Function.h"
#include "helpers.h"
#include "llvm/IR/Constants.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/InstrTypes.h"
#include <bits/stdc++.h>

using namespace llvm;
using namespace LLVMJsonBackend;

namespace LLBMV2 {

Util::IJson* ParserConverter::convertParserStatement(Instruction* inst) {
    auto result = new Util::JsonObject();
    auto params = mkArrayField(result, "parameters");
    if (isa<StoreInst>(inst)) {
        //errs() << "processing store for parser-states\n" << *inst << "\n";
        auto assign = dyn_cast<StoreInst>(inst);
        cstring operation = "set";
        auto left_field = new Util::JsonArray();
        getFieldName(assign->getOperand(1), left_field);
        auto right = getJsonExp(assign->getOperand(0));
        if(dyn_cast<PointerType>(assign->getOperand(1)->getType())->getElementType()->isStructTy())
            operation = "assign_header";
        result->emplace("op", operation);
        params->append((new Util::JsonObject())->emplace("type", "field")->emplace("value", left_field));
        params->append(right);
    //     auto l = conv->convertLeftValue(assign->left);
    //     bool convertBool = type->is<IR::Type_Boolean>();
    //     auto r = conv->convert(assign->right, true, true, convertBool);
    //     params->append(l);
    //     params->append(r);

    //     if (operation != "set") {
    //         // must wrap into another outer object
    //         auto wrap = new Util::JsonObject();
    //         wrap->emplace("op", "primitive");
    //         auto params = mkParameters(wrap);
    //         params->append(result);
    //         result = wrap;
    //     }

        return result;
    } else if (isa<CallInst>(inst)) {
        auto mce = dyn_cast<CallInst>(inst);
        auto minst = mce->getCalledFunction()->getName();
        int argCount = mce->getFunctionType()->getFunctionNumParams();
        if (minst.contains("extract")) {
            assert(argCount == 1 && 
                "Two argument in extract function, need to convert to one argument. Do it when it hits");
            if (argCount == 1 || argCount == 2) {
                cstring ename = argCount == 1 ? "extract" : "extract_VL";
                result->emplace("op", ename);
                auto arg = mce->getCalledFunction()->arg_begin();
                auto argtype = mce->getFunctionType()->getFunctionParamType(0);
                // Ideally, the condtion checks whether arg is of header type,
                // At this point it should be sure that argtype will be Type_header
                if (argtype->isPointerTy() && !argtype->getPointerElementType()->isStructTy()) {
                    errs() << "ERROR : extract only accepts arguments with header types, not : "
                            << argtype->getTypeID() << "\n";
                    exit(1); 
                    return result;
                }
                auto param = new Util::JsonObject();
                params->append(param);
                cstring type;
                // Util::IJson* j = nullptr;

                // if (arg->expression->is<IR::Member>()) {
                //     auto mem = arg->expression->to<IR::Member>();
                //     auto baseType = typeMap->getType(mem->expr, true);
                //     if (baseType->is<IR::Type_Stack>()) {
                //         if (mem->member == IR::Type_Stack::next) {
                //             type = "stack";
                //             j = conv->convert(mem->expr);
                //         } else {
                //             BUG("%1%: unsupported", mem);
                //         }
                //     }
                // }
                // if (j == nullptr) {
                type = "regular";
                // j = conv->convert(arg->expression);
                // auto field = new Util::JsonArray();
                // getFieldName(inst->getOperand(0), field);
                auto field = getFieldName(inst->getOperand(0)).substr(1);
                // }
                // auto value = j->to<Util::JsonObject>()->get("value");
                // auto value 
                param->emplace("type", type);
                param->emplace("value", field);

                // if (argCount == 2) {
                //     auto arg2 = mce->arguments->at(1);
                //     auto jexpr = conv->convert(arg2->expression, true, false);
                //     auto rwrap = new Util::JsonObject();
                //     // The spec says that this must always be wrapped in an expression
                //     rwrap->emplace("type", "expression");
                //     rwrap->emplace("value", jexpr);
                //     params->append(rwrap);
                // }
                return result;
            }
        } else if (minst.contains("verify")) {
            assert(false && "verify is not handled, errors need fixing in frontend");
            // auto extfn = minst->to<P4::ExternFunction>();
            // if (extfn->method->name.name == IR::ParserState::verify) {
            // result->emplace("op", "verify");
            // BUG_CHECK(mce->arguments->size() == 2, "%1%: Expected 2 arguments", mce);
            // if(argCount != 2) {
            //     errs() << "ERROR : Expected 2 arguments in verify function\n";
            //     exit(1); 
            // }
            // {
            //     auto cond = mce->arguments->at(0);
            //     // false means don't wrap in an outer expression object, which is not needed
            //     // here
            //     auto jexpr = conv->convert(cond->expression, true, false);
            //     params->append(jexpr);
            // }
            // {
            //     auto error = mce->arguments->at(1);
            //     // false means don't wrap in an outer expression object, which is not needed
            //     // here
            //     auto jexpr = conv->convert(error->expression, true, false);
            //     params->append(jexpr);
            // }
            return result;
            // }
        } else if(minst.contains("setValid") || minst.contains("setInvalid")) {
            result->emplace("op", "primitive");
            auto paramsValue = new Util::JsonObject();
            params->append(paramsValue);
            auto pp = mkArrayField(paramsValue, "parameters");
            assert(argCount == 1 && "setValid/setInvalid function should have one argument");
            cstring primitive;
            if(minst.contains("setValid"))
                primitive = "add_header";
            else if(minst.contains("setInValid"))
                primitive = "remove_header";
            else
                assert(false && "This should not happen");

            auto argtype = mce->getFunctionType()->getFunctionParamType(0);
            // Ideally, the condtion checks whether arg is of header type,
            // At this point it should be sure that argtype will be Type_header
            if (!argtype->isStructTy()) {
                errs() << "extract only accepts arguments with header types, not : "
                       << argtype->getTypeID() << "\n";
                exit(1);
                return result;
            }
            auto obj = new Util::JsonObject();
            pp->append(obj->emplace("type", "header"));
            pp->append(obj->emplace("value", getFieldName(inst->getOperand(0)).substr(1).c_str()));
            paramsValue->emplace("op", primitive);
            return result;
        } 
        // else if (minst->is<P4::BuiltInMethod>()) {
        //     /* example result:
        //      {
        //         "parameters" : [
        //         {
        //           "op" : "add_header",
        //           "parameters" : [{"type" : "header", "value" : "ipv4"}]
        //         }
        //       ],
        //       "op" : "primitive"
        //     } */
        //     result->emplace("op", "primitive");

        //     auto bi = minst->to<P4::BuiltInMethod>();
        //     cstring primitive;
        //     auto paramsValue = new Util::JsonObject();
        //     params->append(paramsValue);

        //     auto pp = mkArrayField(paramsValue, "parameters");
        //     auto obj = conv->convert(bi->appliedTo);
        //     pp->append(obj);

        //     if (bi->name == IR::Type_Header::setValid) {
        //         primitive = "add_header";
        //     } else if (bi->name == IR::Type_Header::setInvalid) {
        //         primitive = "remove_header";
        //     } else if (bi->name == IR::Type_Stack::push_front ||
        //                bi->name == IR::Type_Stack::pop_front) {
        //         if (bi->name == IR::Type_Stack::push_front)
        //             primitive = "push";
        //         else
        //             primitive = "pop";

        //         BUG_CHECK(mce->arguments->size() == 1, "Expected 1 argument for %1%", mce);
        //         auto arg = conv->convert(mce->arguments->at(0)->expression);
        //         pp->append(arg);
        //     } else {
        //         BUG("%1%: Unexpected built-in method", bi->name);
        //     }

        //     paramsValue->emplace("op", primitive);
        //     return result;
        // }
    }
    // ::error("%1%: not supported in parser on this target", stat);
    errs() << "ERROR : This type is not supported in parser\n" << *inst <<"\n";
    exit(1);
    return result;
}

bool ParserConverter::processParser(llvm::Function *F) {
    auto parser_id = json->add_parser("parser");
    // convert parse state
    for (auto state = F->begin(); state != F->end(); state++) {
        if (dyn_cast<Value>(state)->getName() == "reject" ||
            dyn_cast<Value>(state)->getName() == "accept")
            continue;
        auto state_id = json->add_parser_state(parser_id, state->getName().str());
        // convert statements
        for (auto inst = state->begin(); inst != state->end(); inst++) {
            if(isa<StoreInst>(&*inst) || isa<CallInst>(&*inst)) {
                auto op = convertParserStatement(&*inst);
                json->add_parser_op(state_id, op);
            }
        }
        // convert transitions
        auto term = state->getTerminator();
        unsigned successors = term->getNumSuccessors();
        if(successors > 1) {
            if(auto switch_inst = dyn_cast<SwitchInst>(term)) {
                for(unsigned s = 0; s < successors; s++) {
                    auto trans = new Util::JsonObject();
                    BasicBlock* def_bb = switch_inst->getDefaultDest();
                    if(def_bb == term->getSuccessor(s))
                        trans->emplace("value", "default");
                    else {
                        std::stringstream stream;
                        stream << "0x" << std::setfill('0') << std::setw(switch_inst->findCaseDest(term->getSuccessor(s))->getBitWidth()/4) << std::hex
                               << switch_inst->findCaseDest(term->getSuccessor(s))->getZExtValue();
                        std::string result(stream.str());
                        trans->emplace("value", result.c_str());
                    }
                    trans->emplace("mask", Util::JsonValue::null);
                    if (dyn_cast<Value>(term->getSuccessor(s))->getName().str() == "accept" ||
                        dyn_cast<Value>(term->getSuccessor(s))->getName().str() == "reject")
                        trans->emplace("next_state", Util::JsonValue::null);
                    else
                        trans->emplace("next_state", term->getSuccessor(s)->getName().str().c_str());
                    json->add_parser_transition(state_id, trans);
                }
               auto transition_key_arr = new Util::JsonArray();
               // errs() << "trans key: " <<getFieldName(switch_inst->getCondition(), transition_key_arr) << "\n";
               auto transition_key = new Util::JsonObject();
               transition_key->emplace("type", "field");
               transition_key->emplace("value", transition_key_arr);
               json->add_parser_transition_key(state_id, (new Util::JsonArray())->append(transition_key));
            } else if(BranchInst* cond_branch = dyn_cast<BranchInst>(term)) {
                // errs() << "conditional branch \n" << *cond_branch << "\n";
                if(cond_branch->isConditional()) {
                    Value *cond = cond_branch->getCondition();
                    if(auto icmp = dyn_cast<ICmpInst>(cond)) {
                        if(icmp->isEquality()) {
                            int64_t jmpVal;
                            if(isa<ConstantInt>(icmp->getOperand(0)) &&
                                !isa<ConstantInt>(icmp->getOperand(1))) {
                                jmpVal = dyn_cast<ConstantInt>(icmp->getOperand(0))->getZExtValue();
                            } else if (!isa<ConstantInt>(icmp->getOperand(0)) &&
                                isa<ConstantInt>(icmp->getOperand(1))) {
                                jmpVal = dyn_cast<ConstantInt>(icmp->getOperand(1))->getZExtValue();
                            } else {
                                assert(false && "non-const value in switch case, should not happen");
                                return false;
                            }
                            std::stringstream stream;
                            stream << "0x" << std::setfill('0') << std::setw(dyn_cast<ConstantInt>(icmp->getOperand(1))->getBitWidth()/4) << std::hex
                                   << jmpVal;
                            std::string jmpVal_hex(stream.str());

                            auto trans1 = new Util::JsonObject();
                            trans1->emplace("value", jmpVal_hex);
                            trans1->emplace("mask", Util::JsonValue::null);
                            if (dyn_cast<Value>(term->getSuccessor(0))->getName().str() == "accept" ||
                                dyn_cast<Value>(term->getSuccessor(0))->getName().str() == "reject") {
                                trans1->emplace("next_state", Util::JsonValue::null);
                            } else {
                                auto next_stage = dyn_cast<Value>(term->getSuccessor(0))->getName().str();
                                trans1->emplace("next_state", next_stage);
                            }
                            json->add_parser_transition(state_id, trans1);

                            auto trans2 = new Util::JsonObject();
                            trans2->emplace("value", "default");
                            trans2->emplace("mask", Util::JsonValue::null);
                            if (dyn_cast<Value>(term->getSuccessor(1))->getName().str() == "accept" ||
                                dyn_cast<Value>(term->getSuccessor(1))->getName().str() == "reject") {
                                trans2->emplace("next_state", Util::JsonValue::null);
                            } else {
                                auto next_stage = dyn_cast<Value>(term->getSuccessor(1))->getName().str();
                                trans2->emplace("next_state", next_stage);
                            }
                            json->add_parser_transition(state_id, trans2);
                        }
                        else {
                            assert(false && "non equality in parser branch conditon, should not happen");
                            return false;
                        }
                    }
                    else {
                        assert(false && "parser branch is not icmp, should not happen");
                        return false;
                    }
                }
                auto transition_key_arr = new Util::JsonArray();
                auto comp_inst = dyn_cast<Instruction>(cond_branch->getCondition());
                for(auto i = 0; i < comp_inst->getNumOperands(); i++) {
                    if (!isa<Constant>(comp_inst->getOperand(i)))
                        getFieldName(comp_inst->getOperand(i), transition_key_arr);
                }
                auto transition_key = new Util::JsonObject();
                transition_key->emplace("type", "field");
                transition_key->emplace("value", transition_key_arr);
                json->add_parser_transition_key(state_id,  (new Util::JsonArray())->append(transition_key));
            }
            else {
                assert(false && "Never come here");
                return false;
            }

        } else if(successors == 1) {
            auto trans = new Util::JsonObject();
            trans->emplace("value", "default");
            trans->emplace("mask", Util::JsonValue::null);
            auto next_state = dyn_cast<Value>(term->getSuccessor(0))->getName().str();
            trans->emplace("next_state", next_state);
            json->add_parser_transition(state_id, trans);
        }else {
            return false;
        }
    }
    return false;
}

}  // namespace LLBMV2
