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

#include "emitAction.h"

using namespace llvm;
using namespace LLVMJsonBackend;
namespace LLBMV2 {

/// return false if the value stored is a parameter of the function
bool ConvertActions::isAssignment(StoreInst *st) {
    if(isa<Argument>(st->getOperand(0)) && isa<AllocaInst>(st->getOperand(1)))
        return false;
    else
        return true;
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
            if(!isAssignment(assign))
                continue;
            cstring operation;
            auto left_arr = new Util::JsonArray();
            auto left = getFieldName(assign->getOperand(1), left_arr).substr(1);
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
            left_exp->emplace("value", left_arr);
            parameters->append(left_exp);

            auto right = new Util::JsonObject();
            auto right_val = getJsonExp(assign->getOperand(0));
            auto right_param = getFieldName(assign->getOperand(0));
            if(strlen(right_param) > 0 && isActionParam(right_param.c_str())) {
                auto id = getRuntimeID(right_param.c_str());
                right->emplace("type", "runtime_data");
                right->emplace("value", id);
                parameters->append(right);
            }
            else {
                auto right_exp = new Util::JsonObject();
                right_exp->emplace("type", "expression");
                right_exp->emplace("value", right_val);
                parameters->append(right_exp);
            }
            continue;
        }
        else if (isa<CallInst>(I))
        {
            auto call = dyn_cast<CallInst>(I);
            auto callName = call->getCalledFunction()->getName();
            int argCount = call->getFunctionType()->getFunctionNumParams();
            cstring prim;
            auto parameters = new Util::JsonArray();
            if(callName.contains("setValid") || callName.contains("setInvalid")) {
                if (callName.contains("setValid"))
                {
                    prim = "add_header";
                }
                else if (callName.contains("setInvalid"))
                {
                    prim = "remove_header";
                }
                // auto obj = new Util::JsonArray();
                // getFieldName(I->getOperand(0), obj);
                auto obj = getFieldName(I->getOperand(0)).substr(1);
                parameters->append((new Util::JsonObject())->emplace("type", "header")->emplace("value", obj));

            } else if (callName.contains("mark_to_drop")) {
                prim = "drop";
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

void
ConvertActions::convertActionParams(Function *F, Util::JsonArray* params) {
    // The loops below essentially iterate over Function params
    // First loop gets param through allocas
    // While the second loop iterates on Function params and adds them to Json objects
    // I see a importance for both, if the code is un-optimized, allocas will be there,
    //      to track them we need first loop
    // If the code is optimized then no issue of allocas be happy :)
    for(auto inst = inst_begin(F); inst != inst_end(F); inst++) {
        if(auto st = dyn_cast<StoreInst>(&*inst)) {
            if(!isAssignment(st)) {
                /// This store corresponds to a storing a parameter to a alloca
                auto paramName = (F->getName() + "._" + dyn_cast<Argument>(st->getOperand(0))->getName()).str();
                setAllocaName(dyn_cast<Instruction>(st->getOperand(1)), paramName);
                addToActionParamList(paramName);
            }

        }
    }

    for (auto p = F->arg_begin(); p != F->arg_end(); p++) {
        Argument *arg = &*p;
        if(arg->getType()->isPointerTy())
            continue;
        auto param = new Util::JsonObject();
        auto paramName = (F->getName() + "._" + arg->getName()).str();
        setRuntimeID(paramName,  p - F->arg_begin());
        addToActionParamList(paramName);
        param->emplace("name", paramName);
        auto type = arg->getType();
        if ((type->isVectorTy() && type->getVectorElementType()->isIntegerTy(1)) ||
            (type->isIntegerTy())) {
            if(type->isVectorTy())
                param->emplace("bitwidth", type->getVectorNumElements());
            else if(type->isIntegerTy())
                param->emplace("bitwidth", type->getIntegerBitWidth());
            else
                assert("This should not happen");
            params->append(param);
        }
        else {
            errs() << "ERROR : Action parameters can only be bit<> or int<> on this target\n";
            exit(1);
        }
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
    setActionID(name, id);
    actionParamList.clear();
        // backend->getStructure().ids.emplace(action, id);
}

// bool
// ConvertActions::preorder(const IR::PackageBlock*) {
//     createActions();
//     return false;
// }

}  // namespace BMV2
