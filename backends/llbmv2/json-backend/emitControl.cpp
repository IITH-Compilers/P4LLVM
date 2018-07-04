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

#include "emitControl.h"

using namespace llvm;
using namespace LLVMJsonBackend;

namespace LLBMV2 {

Util::JsonObject* ControlConverter::convertTable(CallInst *apply_call,
                                            cstring table_name,
                                            cstring nex_table_name) {
    auto table = new Util::JsonObject();
    table->emplace("name", table_name);
    table->emplace("id", nextId("tables"));
    table->emplace("source_info", Util::JsonValue::null);
    std::string table_match_type = "exact";
    auto tkey = mkArrayField(table, "key");
    std::vector<std::string> keys;
    unsigned actionsPtrStartIdx;
    for(auto arg = 0; arg < apply_call->getNumOperands()-1; arg = arg+2) {
        if (apply_call->getOperand(arg)->getType()->isPointerTy() &&
            apply_call->getOperand(arg)->getType()->getPointerElementType()->isFunctionTy()) {
                actionsPtrStartIdx = arg;
            break;
        }
        std::string match;
        if(dyn_cast<GlobalVariable>(apply_call->getOperand(arg))->isConstant()) {
            auto cont = dyn_cast<GlobalVariable>(apply_call->getOperand(arg));
            auto cont1 = dyn_cast<ConstantDataArray>(cont->getInitializer());
            match = cont1->getAsString().str().c_str();
        }
        auto key_arr = new Util::JsonArray();
        auto key = getFieldName(apply_call->getOperand(arg+1), key_arr);

        if (match != table_match_type) {
            if (match == "range")
                table_match_type = "range";
            if (match == "ternary" && table_match_type != "range")
                table_match_type = "ternary";
            if (match == "lpm" && table_match_type == "exact")
                table_match_type = "lpm";
        } else if (match == "lpm") {
            errs() << "Multiple LPM keys in table\n";
            exit(1);
        }
        auto keyelement = new Util::JsonObject();
        keyelement->emplace("match_type", match);
        keyelement->emplace("target", key_arr);
        keyelement->emplace("mask", Util::JsonValue::null);
        tkey->append(keyelement);
        
    }
    auto action_ids = new Util::JsonArray();
    auto action_names = new Util::JsonArray();
    std::vector<std::string> actionVector;
    while (apply_call->getOperand(actionsPtrStartIdx)->getType()->isPointerTy() &&
           isa<Function>(apply_call->getOperand(actionsPtrStartIdx))) {
        auto fun = dyn_cast<Function>(apply_call->getOperand(actionsPtrStartIdx));
        auto act_name = fun->getName().str();
        action_names->append(act_name);
        actionVector.push_back(act_name);
        action_ids->append(getActionID(act_name));
        actionsPtrStartIdx++;
    }
    table->emplace("match_type", table_match_type);
    std::string table_type;
    if (auto table_type_call = dyn_cast<CallInst>(apply_call->getOperand(actionsPtrStartIdx++)))
        if(table_type_call->getCalledFunction()->getName() == "simplImpl")
            table_type = "simple";
        else {
            errs() << "ERROR :Unknown table_type function call \n";
            exit(1);
        }
    else {
        errs() << "ERROR :Unknown type in table_type function call \n";
        exit(1);
    }

    table->emplace("type", table_type);

    unsigned table_size;
    if (auto size = dyn_cast<ConstantInt>(apply_call->getOperand(actionsPtrStartIdx++))) {
        table_size = size->getZExtValue();
    } else {
        errs() << "ERROR : Unknown type for table size field\n";
        exit(1);
    }

    cstring default_action;
    if (apply_call->getOperand(actionsPtrStartIdx)->getType()->isPointerTy() &&
        isa<Function>(apply_call->getOperand(actionsPtrStartIdx))) {
        auto fun = dyn_cast<Function>(apply_call->getOperand(actionsPtrStartIdx));
        default_action = fun->getName().str();
    } else
        assert(false && "No default action");

    if(std::find(actionVector.begin(), actionVector.end(), default_action.c_str()) == actionVector.end()) {
        action_names->append(default_action);
        action_ids->append(getActionID(default_action));
    }

    auto default_entry = new Util::JsonObject();
    default_entry->emplace("action_id", getActionID(default_action));
    default_entry->emplace("action_const", false);
    default_entry->emplace("action_data", new Util::JsonArray());
    default_entry->emplace("action_entry_const", false);

    // code for next_tables
    auto next_tables = new Util::JsonObject();
    if(nex_table_name != cstring::empty)
        for(auto act : actionVector) {
            next_tables->emplace(act, nex_table_name);
        }
    else
        for(auto act : actionVector) {
            next_tables->emplace(act, Util::JsonValue::null);
        }

    table->emplace("max_size", table_size);
    table->emplace("with_counters", false);
    table->emplace("support_timeout", false);
    table->emplace("direct_meters", Util::JsonValue::null);
    table->emplace("action_ids", action_ids);
    table->emplace("actions", action_names);
    if (nex_table_name == cstring::empty)
        table->emplace("base_default_next", Util::JsonValue::null);
    else
        table->emplace("base_default_next", nex_table_name);
    table->emplace("next_tables", next_tables);
    table->emplace("default_entry", default_entry);
    return table;
}

cstring ControlConverter::getApplyCallName(BasicBlock *bb)  {
    for(auto I = bb->begin(); I != bb->end(); I++) {
        auto inst = &*I;
        if(auto call_inst = dyn_cast<CallInst>(inst)) {
            if (call_inst->getCalledFunction()->getName().contains("apply_")) {
                return call_inst->getCalledFunction()->getName().split("_").second.str().c_str();
            }
        }
    }
    return nullptr;
}

void ControlConverter::convertConditional(BranchInst *bi, Util::JsonArray* conditionals) {
    if(condMap.find(bi->getParent()) != condMap.end())
        return;
    auto conditional = new Util::JsonObject();
    auto cond_name = genName("cond_");
    condMap[bi->getParent()] = cond_name;
    conditional->emplace("name", cond_name);
    conditional->emplace("id", nextId("conditionals"));
    conditional->emplace("source_info", Util::JsonValue::null);
    auto pred = getJsonExp(bi->getCondition());
    conditional->emplace("expression", pred);
    bool found_apply = false;
    cstring true_branch, false_branch;
    true_branch = getApplyCallName(bi->getSuccessor(0));
    if(true_branch != nullptr)
        found_apply = true;

    if(!found_apply && bi->getSuccessor(0)->getTerminator()->getNumSuccessors() > 1) {
        convertConditional(dyn_cast<BranchInst>(bi->getSuccessor(0)->getTerminator()), conditionals);
        true_branch = condMap[bi->getSuccessor(0)];
    } else if(!found_apply && bi->getSuccessor(0)->getTerminator()->getNumSuccessors() == 1) {
        auto bb = bi->getSuccessor(0)->getTerminator()->getSuccessor(0);
        while(bb) {
            if(getApplyCallName(bb) != nullptr) {
                true_branch = getApplyCallName(bb);
                bb = nullptr;
            } else if(isa<ReturnInst>(bb->getTerminator())) {
                true_branch = "";
                bb = nullptr;
            } else if (auto bbi = dyn_cast<BranchInst>(bb->getTerminator())) {
                if(bbi->isConditional()) {
                    convertConditional(bbi, conditionals);
                    true_branch = condMap[bb];
                    bb = nullptr;
                } else {
                    bb = bb->getTerminator()->getSuccessor(0);
                }
            }
        }
    }
    if(true_branch == nullptr)
        conditional->emplace("true_next", Util::JsonValue::null);
    else
        conditional->emplace("true_next", true_branch);
    found_apply = false;
    false_branch = getApplyCallName(bi->getSuccessor(1));
    if(false_branch != nullptr)
        found_apply = true;
    if(!found_apply && bi->getSuccessor(1)->getTerminator()->getNumSuccessors() > 1) {
        convertConditional(dyn_cast<BranchInst>(bi->getSuccessor(1)->getTerminator()), conditionals);
        false_branch = condMap[bi->getSuccessor(1)];
    } else if(!found_apply && bi->getSuccessor(1)->getTerminator()->getNumSuccessors() == 1) {
        auto bb = bi->getSuccessor(1)->getTerminator()->getSuccessor(0);
        while(bb) {
            if(getApplyCallName(bb) != nullptr) {
                false_branch = getApplyCallName(bb);
                bb = nullptr;
            } else if(isa<ReturnInst>(bb->getTerminator())) {
                false_branch = "";
                bb = nullptr;
            } else if (auto bbi = dyn_cast<BranchInst>(bb->getTerminator())) {
                if(bbi->isConditional()) {
                    convertConditional(bbi, conditionals);
                    false_branch = condMap[bb];
                    bb = nullptr;
                } else {
                    bb = bb->getTerminator()->getSuccessor(0);
                }
            }
        }
    }
    if(false_branch == nullptr)
        conditional->emplace("false_next", Util::JsonValue::null);
    else
        conditional->emplace("false_next", false_branch);
    conditionals->append(conditional);
    return;
}

void ControlConverter::processControl(Function* F) {
    auto pipeline = new Util::JsonObject();
    pipeline->emplace("name", F->getName());
    pipeline->emplace("id", nextId("control"));
    pipeline->emplace("source_info", Util::JsonValue::null);
    auto conditionals = mkArrayField(pipeline, "conditionals");
    std::vector<CallInst*> *apply_calls = new std::vector<CallInst*>();
    bool init_table_set = false;
    bool hasConditional = false;
    auto tables = mkArrayField(pipeline, "tables");
    for(auto BB = F->begin(); BB != F->end(); BB++) {
        apply_calls->clear();
        hasConditional = false;
        auto bb = &*BB;
        for(auto c = bb->begin(); c != bb->end(); c++) {
            if(auto cinst = dyn_cast<CallInst>(&*c))
                if(cinst->getCalledFunction()->getName().contains("apply_"))
                    apply_calls->push_back(cinst);
        }
        cstring cur_table_name, nex_table_name;

        if(apply_calls->size() == 0) {
            if(isa<ReturnInst>(bb->getTerminator()) && !init_table_set)
                if(!init_table_set) {
                    pipeline->emplace("init_table", Util::JsonValue::null);
                    init_table_set = true;
                }
        } else {
            auto apply_call_name = (*(apply_calls->begin()))->getCalledFunction()->getName();
            cur_table_name = apply_call_name.split("_").second.str().c_str();
            if (!init_table_set) {
                pipeline->emplace("init_table", cur_table_name);
                init_table_set = true;
            }
        }
        // Addtion of conditional start here
        bool found_apply = false;
        if (bb->getTerminator()->getNumSuccessors() > 1) {
            hasConditional = true;
            if(condMap.find(bb) == condMap.end()) {
                convertConditional(dyn_cast<BranchInst>(bb->getTerminator()), conditionals);
            }
            if(!init_table_set) {
                init_table_set = true;
                pipeline->emplace("init_table", condMap[bb]);
            }
        } else if (bb->getTerminator()->getNumSuccessors() == 1) {
            auto succr = bb->getTerminator()->getSuccessor(0);
            while(1) {
                if(getApplyCallName(succr) != nullptr) {
                    condMap[bb] = getApplyCallName(succr);
                    break;
                }else if(succr->getTerminator()->getNumSuccessors() > 1) {
                    if(condMap.find(bb) == condMap.end()) {
                        convertConditional(dyn_cast<BranchInst>(succr->getTerminator()), conditionals);
                        condMap[bb] = condMap[succr];
                    }
                    break;
                }else if(succr->getTerminator()->getNumSuccessors() == 1){
                    succr = succr->getTerminator()->getSuccessor(0);
                } else{
                    if(isa<ReturnInst>(succr->getTerminator()))
                        condMap[bb] = cstring::empty;
                    else
                        assert(false && "unknown case");
                    break;
                }
            }
        }

        // Addition of tables start here
        Util::JsonObject* ret;
        for(auto cinst = apply_calls->begin(); cinst != apply_calls->end(); cinst++) {
            if((cinst+1) == apply_calls->end()) {
                if(condMap.find(bb) != condMap.end() && condMap[bb] != cstring::empty)
                   ret = convertTable(*cinst, cur_table_name, condMap[bb]);
                else
                   ret = convertTable(*cinst, cur_table_name, cstring::empty);
            } else {
                nex_table_name = (*(cinst+1))->getCalledFunction()->getName().split("_").second.str().c_str();
                ret = convertTable(*cinst, cur_table_name, nex_table_name);
            }
            tables->append(ret);
            cur_table_name = nex_table_name;
        }
    }
    // Addition of action_profiles
    auto action_profiles = mkArrayField(pipeline, "action_profiles");

    json->pipelines->append(pipeline);
}

cstring ChecksumConverter::createCalculation(cstring algo, std::vector<Value*> args,
                        Util::JsonArray* calculations, bool withPayload) {
    cstring calcName = genName("calc_");
    auto calc = new Util::JsonObject();
    calc->emplace("name", calcName);
    calc->emplace("id", nextId("calculations"));
    calc->emplace("source_info", Util::JsonValue::null);
    auto input = new Util::JsonArray();
    for(auto arg : args) {
        auto arg_obj = new Util::JsonObject();
        arg_obj->emplace("type", "field");
        auto arg_obj_arr = new Util::JsonArray();
        getFieldName(arg, arg_obj_arr);
        arg_obj->emplace("value", arg_obj_arr);
        input->append(arg_obj);
    }
    calc->emplace("algo", algo);
    calc->emplace("input", input);
    calculations->append(calc);
    return  calcName;
}

void ChecksumConverter::processChecksum(Function *F) {

    for(auto inst = inst_begin(F); inst != inst_end(F); inst++) {
        Instruction *I = &*inst;
        if(isa<CallInst>(&*inst) && inst->getType()->isVoidTy()) {
            auto call_inst = dyn_cast<CallInst>(&*inst);
            auto calledFun = call_inst->getCalledFunction();
            assert(I->getNumOperands() > 2 && "Not enough opernads in verify/update checksum");
            bool usePayload = I->getName().contains("_with_payload");
            auto cksum = new Util::JsonObject();
            std::vector<Value*> data;
            for(auto op = 1; op < I->getNumOperands()-2; op++) {
                data.push_back(I->getOperand(op));
            }
            cstring calcName = createCalculation("csum16", data,
                                                json->calculations, usePayload);
            cksum->emplace("name", genName("cksum_"));
            cksum->emplace("id", nextId("checksums"));
            auto jleft = new Util::JsonArray();
            getFieldName(I->getOperand(I->getNumOperands()-2), jleft);
            cksum->emplace("target", jleft);
            cksum->emplace("type", "generic");
            cksum->emplace("calculation", calcName);
            auto bool_arg = I->getOperand(0);
            auto ifcond = new Util::JsonObject();
            if(auto bool_call = dyn_cast<CallInst>(bool_arg)) {
                if (bool_call->getCalledFunction()->getName() == "isValid" ||
                    bool_call->getCalledFunction()->getName() == "isInvalid") {
                        ifcond->emplace("type", "expression");
                        auto value = new Util::JsonObject();
                        value->emplace("op", "d2b");
                        value->emplace("left", Util::JsonValue::null);
                        auto right_arr = new Util::JsonArray();
                        getFieldName(dyn_cast<Instruction>(bool_arg)->getOperand(0), right_arr);
                        value->emplace("right", right_arr);
                        ifcond->emplace("value", value);
                    }
            } else if(auto bool_const = dyn_cast<ConstantInt>(bool_arg)) {
                auto val = bool_const->getZExtValue(); 
                ifcond->emplace("type", "bool");
                if(val == 0)
                    ifcond->emplace("value", "false");
                else
                    ifcond->emplace("value", true);
            }
            // auto ifcond = conv->convert(mi->expr->arguments->at(0), true, false);
            cksum->emplace("if_cond", ifcond);
            json->checksums->append(cksum);
            continue;
        }
    }
    
    return;
}

}  // namespace BMV2
