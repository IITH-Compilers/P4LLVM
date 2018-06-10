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

#include "emitAction.h"

using namespace llvm;
namespace LLBMV2 {

Util::IJson *ConvertActions::getJsonExp(Value *inst)
{
    errs() << "inst into getJsonExp is\n" << *inst << "\n";
    auto result = new Util::JsonObject();
    if (auto bo = dyn_cast<BinaryOperator>(inst))
    {
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
        stream << std::hex << (std::pow(2, bw) - 1);
        auto trunc = new Util::JsonObject();
        trunc->emplace("left", result_ex);
        auto trunc_val = new Util::JsonObject();
        trunc_val->emplace("type", "hexstr");
        trunc_val->emplace("value", stream.str().c_str());
        trunc->emplace("right", trunc_val);
        trunc->emplace("op", "&");
        auto trunc_exp = new Util::JsonObject();
        trunc_exp->emplace("type", "expression");
        trunc_exp->emplace("value", trunc);
        return trunc_exp;
    }
    else if(auto sel = dyn_cast<SelectInst>(inst)) {
        if(sel->getType()->isIntegerTy(1)) {
            result->emplace("op", "?");
            auto left = dyn_cast<ConstantInt>(sel->getOperand(1))->getZExtValue();
            auto right = dyn_cast<ConstantInt>(sel->getOperand(2))->getZExtValue();
            auto cond = getJsonExp(sel->getOperand(0));
            auto left_exp = new Util::JsonObject();
            left_exp->emplace("type", "hexstr");
            left_exp->emplace("value", left);
            auto right_exp = new Util::JsonObject();
            right_exp->emplace("type", "hexstr");
            right_exp->emplace("value", right);
            result->emplace("left", left_exp);
            result->emplace("right", right_exp);
            result->emplace("cond", cond);
            auto result_ex = new Util::JsonObject();
            result_ex->emplace("type", "expression");
            result_ex->emplace("value", result);
            return result_ex;
        }
        else
            assert(false && "select inst with non boolean not handled");
    }
    else if (auto cmp  = dyn_cast<ICmpInst>(inst))
    {
        errs() << "number of operands in icmp are : " << cmp->getNumOperands() << "\n";
        auto pred = CmpInst::getPredicateName(cmp->getSignedPredicate()).str();
        errs() << "predicate name: " << pred << "\n";
        cstring op;
        if(pred == "eq")
            op = "==";
        else if(pred == "ne")
            op = "!=";
        else if(pred == "sge" || pred == "uge")
            op = ">=";
        else if(pred == "sle" || pred == "ule")
            op = "<=";
        else if(pred == "sgt" || pred == "ugt")
            op = ">";
        else if(pred == "slt" || pred == "ult")
            op = "<";
        else
        {
            errs() << *inst << "\n";
            assert(false && "Unknown operation in ICMP");
        }
        auto left = getJsonExp(cmp->getOperand(0));
        auto right = getJsonExp(cmp->getOperand(1));
        result->emplace("op", op);
        result->emplace("left", left);
        result->emplace("right", right);
        auto result_ex = new Util::JsonObject();
        result_ex->emplace("type", "expression");
        result_ex->emplace("value", result);
        return result_ex;
    }
    else if (auto cnst = dyn_cast<ConstantInt>(inst))
    {
        std::stringstream stream;
        stream << std::hex << cnst->getSExtValue();
        result->emplace("hexstr", stream.str().c_str());
        return result;
    }
    else if (auto ld = dyn_cast<LoadInst>(inst))
    {
        std::string headername = getFieldName(ld->getOperand(0)).substr(1);
        result->emplace("type", "field");
        result->emplace("field", headername.c_str());
        return result;
    }
    else if (auto bc = dyn_cast<BitCastInst>(inst))
    {
        return getJsonExp(bc->getOperand(0));
    }
    else if (auto ze = dyn_cast<ZExtInst>(inst))
    {
        auto left = getJsonExp(ze->getOperand(0));
        auto bw = ze->getType()->getIntegerBitWidth();
        result->emplace("op", "&");
        result->emplace("left", left);
        std::stringstream stream;
        stream << std::hex << (std::pow(2, bw) - 1);
        auto trunc_val = new Util::JsonObject();
        trunc_val->emplace("type", "hexstr");
        trunc_val->emplace("value", stream.str().c_str());
        result->emplace("right", trunc_val);
        auto result_ex = new Util::JsonObject();
        result_ex->emplace("type", "expression");
        result_ex->emplace("value", result);
        return result_ex;
    }
    else
    {
        errs() << *inst << "\n"
               << "ERROR : Unhandled instrution in getJsonExp\n";
        assert(false);
        return result;
    }
}

void
ConvertActions::convertActionBody(Function * F, Util::JsonArray * result)
{
    for (auto inst = inst_begin(F); inst != inst_end(F); inst++)
    {
        Instruction *I = &*inst;
        if (isa<StoreInst>(I))
        {
            auto assign = dyn_cast<StoreInst>(I);
            cstring operation;
            auto left = getFieldName(assign->getOperand(1)).substr(1);
            auto right = getJsonExp(assign->getOperand(0));
            if (getBasicHeaderType(left) == Union)
                operation = "assign_union";
            else if (getBasicHeaderType(left) == Header)
                operation = "assign_header";
            else 
                operation = "assign";
            auto primitive = mkPrimitive(operation, result);
            auto parameters = mkParameters(primitive);
            primitive->emplace("source_info", Util::JsonValue::null);
            auto left_exp = new Util::JsonObject();
            left_exp->emplace("type", "field");
            left_exp->emplace("value", left);
            parameters->append(left_exp);
            parameters->append(right);
            continue;
        }
        else if (isa<CallInst>(I))
        {
            auto call = dyn_cast<CallInst>(I);
            auto callName = call->getCalledFunction()->getName();
            int argCount = call->getFunctionType()->getFunctionNumParams();
            cstring prim;
            auto parameters = new Util::JsonArray();
            auto obj = getFieldName(I->getOperand(0)).substr(1).c_str();
            parameters->append(obj);
            if(callName.contains("setValid") || callName.contains("setInvalid")) {
                if (callName == "setValid")
                {
                    prim = "add_header";
                }
                else if (callName == "setInvalid")
                {
                    prim = "remove_header";
                }

            }
            auto primitive = mkPrimitive(prim, result);
            primitive->emplace("parameters", parameters);
            primitive->emplace("source_info", Util::JsonValue::null);
            continue;
        }
        else
            continue;
    }
}
    // for (auto s : *body)
    // {
        // TODO(jafingerhut) - add line/col at all individual cases below,
        // or perhaps it can be done as a common case above or below
        // for all of them?
        // if (!s->is<IR::Statement>()) {
        //     continue;
        // } else if (auto block = s->to<IR::BlockStatement>()) {
        //     convertActionBody(&block->components, result);
        //     continue;
        // } else if (s->is<IR::ReturnStatement>()) {
        //     break;
    // } else if (s->is<IR::ExitStatement>()) {
    //     auto primitive = mkPrimitive("exit", result);
    //     (void)mkParameters(primitive);
    //     primitive->emplace_non_null("source_info", s->sourceInfoJsonObj());
    //     break;
    // } else if (s->is<IR::AssignmentStatement>()) {
    //     const IR::Expression* l, *r;
    //     auto assign = s->to<IR::AssignmentStatement>();
    //     l = assign->left;
    //     r = assign->right;

    //     cstring operation;
    //     auto type = typeMap->getType(l, true);
    //     if (type->is<IR::Type_Varbits>())
    //         operation = "assign_VL";
    //     else if (type->is<IR::Type_HeaderUnion>())
    //         operation = "assign_union";
    //     else if (type->is<IR::Type_StructLike>())
    //         operation = "assign_header";
    //     else
    //         operation = "assign";
    //     auto primitive = mkPrimitive(operation, result);
    //     auto parameters = mkParameters(primitive);
    //     primitive->emplace_non_null("source_info", assign->sourceInfoJsonObj());
    //     auto left = conv->convertLeftValue(l);
    //     parameters->append(left);
    //     bool convertBool = type->is<IR::Type_Boolean>();
    //     auto right = conv->convert(r, true, true, convertBool);
    //     parameters->append(right);
    //     continue;
    // } else if (s->is<IR::EmptyStatement>()) {
    //     continue;
    // } else if (s->is<IR::MethodCallStatement>()) {
    //     LOG3("Visit " << dbp(s));
    //     auto mc = s->to<IR::MethodCallStatement>()->methodCall;
    //     auto mi = P4::MethodInstance::resolve(mc, refMap, typeMap);
    //     if (mi->is<P4::ActionCall>()) {
    //         BUG("%1%: action call should have been inlined", mc);
    //         continue;
    //     } else if (mi->is<P4::BuiltInMethod>()) {
    //         auto builtin = mi->to<P4::BuiltInMethod>();

    //         cstring prim;
    //         auto parameters = new Util::JsonArray();
    //         auto obj = conv->convert(builtin->appliedTo);
    //         parameters->append(obj);

    //         if (builtin->name == IR::Type_Header::setValid) {
    //             prim = "add_header";
    //         } else if (builtin->name == IR::Type_Header::setInvalid) {
    //             prim = "remove_header";
    //         } else if (builtin->name == IR::Type_Stack::push_front) {
    //             BUG_CHECK(mc->arguments->size() == 1, "Expected 1 argument for %1%", mc);
    //             auto arg = conv->convert(mc->arguments->at(0));
    //             prim = "push";
    //             parameters->append(arg);
    //         } else if (builtin->name == IR::Type_Stack::pop_front) {
    //             BUG_CHECK(mc->arguments->size() == 1, "Expected 1 argument for %1%", mc);
    //             auto arg = conv->convert(mc->arguments->at(0));
    //             prim = "pop";
    //             parameters->append(arg);
    //         } else {
    //             BUG("%1%: Unexpected built-in method", s);
    //         }
    //         auto primitive = mkPrimitive(prim, result);
    //         primitive->emplace("parameters", parameters);
    //         primitive->emplace_non_null("source_info", s->sourceInfoJsonObj());
    //         continue;
    //     } else if (mi->is<P4::ExternMethod>()) {
    //         auto em = mi->to<P4::ExternMethod>();
    //         LOG3("P4V1:: convert " << s);
    //         backend->getSimpleSwitch()->convertExternObjects(result, em, mc, s);
    //         continue;
    //     } else if (mi->is<P4::ExternFunction>()) {
    //         auto ef = mi->to<P4::ExternFunction>();
    //         backend->getSimpleSwitch()->convertExternFunctions(result, ef, mc, s);
    //         continue;
    //     }
    // }
    // ::error("%1%: not yet supported on this target", s);
// }
void
ConvertActions::convertActionParams(Function *F, Util::JsonArray* params) {
    for (auto p = F->arg_begin(); p != F->arg_end(); p++) {
        // if (!refMap->isUsed(p))
        //     ::warning("Unused action parameter %1%", p);
        Argument *arg = &*p;
        if(arg->getType()->isPointerTy())
            continue;
        errs() << "######################################arg is : " << *arg << "\n";    
        auto param = new Util::JsonObject();
        param->emplace("name", arg->getName());
        auto type = arg->getType();
        // auto type = typeMap->getType(p, true);
        if ((type->isVectorTy() && type->getVectorElementType()->isIntegerTy(1)) ||
            (type->isIntegerTy())) {
            errs() << "ERROR : Action parameters can only be bit<> or int<> on this target\n";
            exit(1);
        }
        if(type->isVectorTy())
            param->emplace("bitwidth", type->getVectorNumElements());
        else if(type->isIntegerTy())
            param->emplace("bitwidth", type->getIntegerBitWidth());
        else
            assert("This should not happen");
        params->append(param);
    }
}

void
ConvertActions::processActions(Function *F) {
    cstring name;
    if(F->hasName())
        name = F->getName();
    else
        assert(false && "Action function has no name");
    auto params = new Util::JsonArray();
    convertActionParams(F, params);
    auto body = new Util::JsonArray();
    convertActionBody(F, body);
    auto id = json->add_action(name, params, body);
        // backend->getStructure().ids.emplace(action, id);
}

// bool
// ConvertActions::preorder(const IR::PackageBlock*) {
//     createActions();
//     return false;
// }

}  // namespace BMV2
