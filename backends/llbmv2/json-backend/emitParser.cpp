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

#include "emitParser.h"
#include "../JsonObjects.h"
#include "llvm/IR/Function.h"
#include "helpers.h"
#include "llvm/IR/Constants.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/InstrTypes.h"
#include <bits/stdc++.h>

using namespace llvm;
namespace LLBMV2 {

bool ParserConverter::isIndex1D(llvm::GetElementPtrInst *gep) {
    if(gep->hasAllZeroIndices())
        return true;
    if(gep->getNumIndices() > 2)
        return false;
    return true;
}

unsigned ParserConverter::get1DIndex(llvm::GetElementPtrInst *gep) {
    unsigned num = gep->getNumIndices();
    for(auto id = gep->idx_begin(); id !=gep->idx_end(); id++) {
        unsigned val = dyn_cast<ConstantInt>(id)->getZExtValue();
        errs() << "1D index : " << val << "\n";
        if(val != 0)
            return val;
    }
    return 0;
}

std::string ParserConverter::getMultiDimFieldName(llvm::GetElementPtrInst *gep) {
    auto id = gep->idx_begin();
    id++; // ignore the first index as it will be a zero corresponding to the current type
    std::string result;
    auto src_type = gep->getSourceElementType();
    for ( ; id != gep->idx_end(); id++) {
        unsigned val = dyn_cast<ConstantInt>(id)->getZExtValue();
        if(isa<StructType>(src_type))
            result = result + dyn_cast<StructType>(src_type)->getName().str() + ".field_" + std::to_string(val)+ ".";
        else
            assert(false && "This should not happen");
        src_type = dyn_cast<StructType>(src_type)->getElementType(val);
    }
    return result.substr(0, result.length()-2);
}

std::string ParserConverter::getFieldName(Value* arg) {
    errs() << "input inst is \n" << *arg;
    if(! isa<Instruction>(arg) || isa<AllocaInst>(arg))
        return "";
    auto gep = dyn_cast<GetElementPtrInst>(arg);
    if(gep) {
        errs() << "No of operands in gep : " << gep->getNumOperands() << "\n";
        if(isIndex1D(gep)) {
            auto src_type = dyn_cast<StructType>(gep->getSourceElementType());
            assert(src_type != nullptr && "This should not happen");
            return getFieldName(gep->getPointerOperand()) + "." + src_type->getName().str() + ".field_" + std::to_string(get1DIndex(gep));
        }
        else {
            std::string result = getMultiDimFieldName(gep);
            return result + getFieldName(gep);
        }
    }

    if(auto ld = dyn_cast<LoadInst>(arg)) {
        errs() << "No of operands in load : " << ld->getNumOperands() << "\n";
        return getFieldName(ld->getOperand(0));
    }

    if(auto bc = dyn_cast<BitCastInst>(arg)) {
        errs() << "No of operands in bitcast : " << bc->getNumOperands() << "\n";
        return getFieldName(bc->getOperand(0));
    }
}

// cstring ParserConverter::getBinaryOperation(BinaryOperator *inst) {
//     return inst->getOpcodeName();
// }

Util::IJson* ParserConverter::getJsonExp(Value *inst) {
    auto result = new Util::JsonObject();
    if(auto bo = dyn_cast<BinaryOperator>(inst)) {
        // cstring op = getBinaryOperation(bo);
        cstring op = bo->getOpcodeName();
        result->emplace("op", op);
        auto left = getJsonExp(bo->getOperand(0));
        auto right = getJsonExp(bo->getOperand(1));
        result->emplace("left", left);
        result->emplace("right", right);
        // auto fin = new Util::JsonObject();
        assert(inst->getType()->isIntegerTy() && "should be an integer type");
        unsigned bw = inst->getType()->getIntegerBitWidth();
        auto result_ex = new Util::JsonObject();
        result_ex->emplace("type", "expression");
        result_ex->emplace("value", result);
        std::stringstream stream;
        stream << std::hex << (std::pow(2, bw)-1);
        auto trunc = new Util::JsonObject();
        trunc->emplace("left", result_ex);
        auto trunc_val = new Util::JsonObject();
        trunc_val->emplace("hexstr", stream.str().c_str());
        trunc->emplace("right", trunc_val);
        trunc->emplace("op", "&");
        auto trunc_exp = new Util::JsonObject();
        trunc_exp->emplace("type", "expression");
        trunc_exp->emplace("value", trunc);
        return trunc_exp;
    }
    else if(auto cnst = dyn_cast<ConstantInt>(inst)) {
        std::stringstream stream;
        stream << std::hex << cnst->getSExtValue();
        result->emplace("hexstr", stream.str().c_str());
        return result;
    }
    else if (auto ld = dyn_cast<LoadInst>(inst)) {
        std::string headername = getFieldName(ld->getOperand(0)).substr(1);
        result->emplace("field", headername.c_str());
        return result;
    }
    else {
        errs() << *inst << "\n" << "Unhandled instrution in getJsonExp\n";
        assert(false);
        return result;
    }
}

// TODO(hanw) refactor this function
Util::IJson* ParserConverter::convertParserStatement(Instruction* inst) {
    auto result = new Util::JsonObject();
    auto params = mkArrayField(result, "parameters");
    if (isa<StoreInst>(inst)) {
        auto assign = dyn_cast<StoreInst>(inst);
        cstring operation = "set";
        std::string left_field = getFieldName(assign->getOperand(1)).substr(1);
        auto right = getJsonExp(assign->getOperand(0));
        if(dyn_cast<PointerType>(assign->getOperand(1)->getType())->getElementType()->isStructTy())
            operation = "assign_header";
        result->emplace("op", operation);
        params->append((new Util::JsonObject())->emplace("field", left_field));
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
        // errs() << "no of operands in call inst : " << *inst->getOperand(1) << "\n";
        auto mce = dyn_cast<CallInst>(inst);
        // auto minst = P4::MethodInstance::resolve(mce, refMap, typeMap);
        auto minst = mce->getCalledFunction()->getName();
        int argCount = mce->getFunctionType()->getFunctionNumParams();
        if (minst.contains("extract")) {
            // auto extmeth = minst->to<P4::ExternMethod>();
            // if (extmeth->method->name.name == corelib.packetIn.extract.name) {
            assert(argCount == 1 && 
                "Two argument in extract function, need to convert to one argument. Do it when it hits");
            if (argCount == 1 || argCount == 2) {
                cstring ename = argCount == 1 ? "extract" : "extract_VL";
                result->emplace("op", ename);
                auto arg = mce->getCalledFunction()->arg_begin();
                auto argtype = mce->getFunctionType()->getFunctionParamType(0);
                // Ideally, the condtion checks whether arg is of header type,
                // At this point it should be sure that argtype will be Type_header
                if (!argtype->isStructTy()) {
                    // ::error("%1%: extract only accepts arguments with header types, not %2%",
                    //         arg, argtype);
                    errs() << "extract only accepts arguments with header types, not : "
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
                    cstring field = getFieldName(inst->getOperand(0)).substr(1).c_str();
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
                // ::error("%1%: extract only accepts arguments with header types, not %2%",
                //         arg, argtype);
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

// Operates on a select keyset
// void ParserConverter::convertSimpleKey(const IR::Expression* keySet,
//                                        mpz_class& value, mpz_class& mask) const {
//     if (keySet->is<IR::Mask>()) {
//         auto mk = keySet->to<IR::Mask>();
//         if (!mk->left->is<IR::Constant>()) {
//             ::error("%1% must evaluate to a compile-time constant", mk->left);
//             return;
//         }
//         if (!mk->right->is<IR::Constant>()) {
//             ::error("%1% must evaluate to a compile-time constant", mk->right);
//             return;
//         }
//         value = mk->left->to<IR::Constant>()->value;
//         mask = mk->right->to<IR::Constant>()->value;
//     } else if (keySet->is<IR::Constant>()) {
//         value = keySet->to<IR::Constant>()->value;
//         mask = -1;
//     } else if (keySet->is<IR::BoolLiteral>()) {
//         value = keySet->to<IR::BoolLiteral>()->value ? 1 : 0;
//         mask = -1;
//     } else if (keySet->is<IR::DefaultExpression>()) {
//         value = 0;
//         mask = 0;
//     } else {
//         ::error("%1% must evaluate to a compile-time constant", keySet);
//         value = 0;
//         mask = 0;
//     }
// }

// unsigned ParserConverter::combine(const IR::Expression* keySet,
//                                 const IR::ListExpression* select,
//                                 mpz_class& value, mpz_class& mask,
//                                 bool& is_vset, cstring& vset_name) const {
//     // From the BMv2 spec: For values and masks, make sure that you
//     // use the correct format. They need to be the concatenation (in
//     // the right order) of all byte padded fields (padded with 0
//     // bits). For example, if the transition key consists of a 12-bit
//     // field and a 2-bit field, each value will need to have 3 bytes
//     // (2 for the first field, 1 for the second one). If the
//     // transition value is 0xaba, 0x3, the value attribute will be set
//     // to 0x0aba03.
//     // Return width in bytes
//     value = 0;
//     mask = 0;
//     is_vset = false;
//     unsigned totalWidth = 0;
//     if (keySet->is<IR::DefaultExpression>()) {
//         return totalWidth;
//     } else if (keySet->is<IR::ListExpression>()) {
//         auto le = keySet->to<IR::ListExpression>();
//         BUG_CHECK(le->components.size() == select->components.size(),
//                   "%1%: mismatched select", select);
//         unsigned index = 0;

//         bool noMask = true;
//         for (auto it = select->components.begin();
//              it != select->components.end(); ++it) {
//             auto e = *it;
//             auto keyElement = le->components.at(index);

//             auto type = typeMap->getType(e, true);
//             int width = type->width_bits();
//             BUG_CHECK(width > 0, "%1%: unknown width", e);

//             mpz_class key_value, mask_value;
//             convertSimpleKey(keyElement, key_value, mask_value);
//             unsigned w = 8 * ROUNDUP(width, 8);
//             totalWidth += ROUNDUP(width, 8);
//             value = Util::shift_left(value, w) + key_value;
//             if (mask_value != -1) {
//                 noMask = false;
//             } else {
//                 // mask_value == -1 is a special value used to
//                 // indicate an exact match on all bit positions.  When
//                 // there is more than one keyElement, we must
//                 // represent such an exact match with 'width' 1 bits,
//                 // because it may be combined into a mask for other
//                 // keyElements that have their own independent masks.
//                 mask_value = Util::mask(width);
//             }
//             mask = Util::shift_left(mask, w) + mask_value;
//             LOG3("Shifting " << " into key " << key_value << " &&& " << mask_value <<
//                              " result is " << value << " &&& " << mask);
//             index++;
//         }

//         if (noMask)
//             mask = -1;
//         return totalWidth;
//     } else if (keySet->is<IR::PathExpression>()) {
//         auto pe = keySet->to<IR::PathExpression>();
//         auto decl = refMap->getDeclaration(pe->path, true);
//         vset_name = decl->controlPlaneName();
//         is_vset = true;
//         return totalWidth;
//     } else {
//         BUG_CHECK(select->components.size() == 1, "%1%: mismatched select/label", select);
//         convertSimpleKey(keySet, value, mask);
//         auto type = typeMap->getType(select->components.at(0), true);
//         return ROUNDUP(type->width_bits(), 8);
//     }
// }

// Util::IJson* ParserConverter::stateName(IR::ID state) {
//     if (state.name == IR::ParserState::accept) {
//         return Util::JsonValue::null;
//     } else if (state.name == IR::ParserState::reject) {
//         ::warning("Explicit transition to %1% not supported on this target", state);
//         return Util::JsonValue::null;
//     } else {
//         return new Util::JsonValue(state.name);
//     }
// }

// std::vector<Util::IJson*>
// ParserConverter::convertSelectExpression(const IR::SelectExpression* expr) {
//     std::vector<Util::IJson*> result;
//     auto se = expr->to<IR::SelectExpression>();
//     for (auto sc : se->selectCases) {
//         auto trans = new Util::JsonObject();
//         mpz_class value, mask;
//         bool is_vset;
//         cstring vset_name;
//         unsigned bytes = combine(sc->keyset, se->select, value, mask, is_vset, vset_name);
//         if (is_vset) {
//             trans->emplace("type", "parse_vset");
//             trans->emplace("value", vset_name);
//             trans->emplace("mask", mask);
//             trans->emplace("next_state", stateName(sc->state->path->name));
//         } else {
//             if (mask == 0) {
//                 trans->emplace("value", "default");
//                 trans->emplace("mask", Util::JsonValue::null);
//                 trans->emplace("next_state", stateName(sc->state->path->name));
//             } else {
//                 trans->emplace("type", "hexstr");
//                 trans->emplace("value", stringRepr(value, bytes));
//                 if (mask == -1)
//                     trans->emplace("mask", Util::JsonValue::null);
//                 else
//                     trans->emplace("mask", stringRepr(mask, bytes));
//                 trans->emplace("next_state", stateName(sc->state->path->name));
//             }
//         }
//         result.push_back(trans);
//     }
//     return result;
// }

// Util::IJson*
// ParserConverter::convertSelectKey(const IR::SelectExpression* expr) {
//     auto se = expr->to<IR::SelectExpression>();
//     CHECK_NULL(se);
//     auto key = conv->convert(se->select, false);
//     return key;
// }

// Util::IJson*
// ParserConverter::convertPathExpression(const IR::PathExpression* pe) {
//     auto trans = new Util::JsonObject();
//     trans->emplace("value", "default");
//     trans->emplace("mask", Util::JsonValue::null);
//     trans->emplace("next_state", stateName(pe->path->name));
//     return trans;
// }

// Util::IJson*
// ParserConverter::createDefaultTransition() {
//     auto trans = new Util::JsonObject();
//     trans->emplace("value", "default");
//     trans->emplace("mask", Util::JsonValue::null);
//     trans->emplace("next_state", Util::JsonValue::null);
//     return trans;
// }

bool ParserConverter::processParser(llvm::Function *F) {
    // hanw hard-coded parser name assumed by BMv2
    auto parser_id = json->add_parser("parser");

    // for (auto s : parser->parserLocals) {
    //     if (auto inst = s->to<IR::P4ValueSet>()) {
    //         auto bitwidth = inst->elementType->width_bits();
    //         auto name = inst->controlPlaneName();
    //         json->add_parse_vset(name, bitwidth);
    //     }
    // }

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
    }
    return false;
}

/// visit ParserBlock only
// bool ParserConverter::preorder(const IR::PackageBlock* block) {
//     for (auto it : block->constantValue) {
//         if (it.second->is<IR::ParserBlock>()) {
//             visit(it.second->getNode());
//         }
//     }
//     return false;
// }

}  // namespace LLBMV2
