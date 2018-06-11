/*
IITH Compilers
authors: D Tharun, S Venkata
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
namespace LLBMV2 {

Util::IJson* ControlConverter::convertTable(CallInst *apply_call, cstring table_name) {
    auto table = new Util::JsonObject();
    table->emplace("name", table_name);
    table->emplace("id", nextId("tables"));
    table->emplace("source_info", Util::JsonValue::null);
    std::string table_match_type = "exact";
    auto tkey = mkArrayField(table, "key");
    std::vector<std::string> keys;
    std::map<std::string, std::string> keyMatches;
    errs() << "&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&\n";
    errs() << "Function Name : " << apply_call->getCalledFunction()->getName() << "\n";
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
            errs() << cont1->getAsString() << "\n";
            match = cont1->getAsString().str().c_str();
        }
        auto key = getFieldName(apply_call->getOperand(arg+1));
        keyMatches[key] = match;

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
        keyelement->emplace("target", key);
        keyelement->emplace("mask", Util::JsonValue::null);
        tkey->append(keyelement);
        
    }
    while (apply_call->getOperand(actionsPtrStartIdx)->getType()->isPointerTy() &&
           apply_call->getOperand(actionsPtrStartIdx)->getType()->getPointerElementType()->isFunctionTy()) {
        actionsPtrStartIdx++;
    }
    table->emplace("match_type", table_match_type);
    std::string table_type;
    errs() << "table type inst is : \n"<< *apply_call->getOperand(actionsPtrStartIdx) << "\n";
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
    errs() << *apply_call->getOperand(actionsPtrStartIdx) << "\n";
    if (auto size = dyn_cast<ConstantInt>(apply_call->getOperand(actionsPtrStartIdx++))) {
        table_size = size->getZExtValue();
    } else {
        errs() << "ERROR : Unknown type for table size field\n";
        exit(1);
    }

        // auto table_size = dyn_cast<ConstantInt>(apply_call->getOperand(actionsPtrStartIdx++))->getZExtValue();
    table->emplace("max_size", table_size);
    table->emplace("with_counters", false);
    table->emplace("support_timeout", false);
    table->emplace("direct_meters", Util::JsonValue::null);
    return table;
}

void ControlConverter::processControl(Function* F) {
    auto pipeline = new Util::JsonObject();
    pipeline->emplace("name", F->getName());
    pipeline->emplace("id", nextId("control"));
    pipeline->emplace("source_info", Util::JsonValue::null);
    std::vector<CallInst*> *apply_calls = new std::vector<CallInst*>();
    for(auto c = inst_begin(F); c != inst_end(F); c++) {
        if(auto cinst = dyn_cast<CallInst>(&*c))
            if(cinst->getCalledFunction()->getName().contains("apply"))
                apply_calls->push_back(cinst);
    }
    cstring table_name;
    if(apply_calls->size() == 0) {
        pipeline->emplace("init_table", Util::JsonValue::null);
    }
    else {
        table_name = genName("table_");
        pipeline->emplace("init_table", table_name);
    }
    // Addition of tables start here
    auto tables = mkArrayField(pipeline, "tables");
    for(auto cinst : *apply_calls) {
        auto ret = convertTable(cinst, table_name);
        tables->append(ret);
        table_name = genName("table_");
    }
    // Addition of action_profiles
    auto action_profiles = mkArrayField(pipeline, "action_profiles");

    // Addition of conditionals
    auto conditionals = mkArrayField(pipeline, "conditionals");

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
        arg_obj->emplace("value", getFieldName(arg));
        input->append(arg_obj);
    }
    calc->emplace("algo", algo);
    calc->emplace("input", input);
    calculations->append(calc);
    return  calcName;
}

// void ChecksumConverter::processVerifyChecksum(Funtion *F) {

// }

void ChecksumConverter::processChecksum(Function *F) {
    // auto it = backend->update_checksum_controls.find(block->container->name);
    // if (it != backend->update_checksum_controls.end()) {
    //     if (backend->target == Target::SIMPLE) {
    //         P4V1::SimpleSwitch* ss = backend->getSimpleSwitch();
    //         ss->convertChecksum(block->container->body, backend->json->checksums,
    //                             backend->json->calculations, false);
    //     }
    // } else {
    //     it = backend->verify_checksum_controls.find(block->container->name);
    //     if (it != backend->verify_checksum_controls.end()) {
    //         if (backend->target == Target::SIMPLE) {
    //             P4V1::SimpleSwitch* ss = backend->getSimpleSwitch();
    //             ss->convertChecksum(block->container->body, backend->json->checksums,
    //                                 backend->json->calculations, true);
    //         }
    //     }
    // }
    for(auto inst = inst_begin(F); inst != inst_end(F); inst++) {
        Instruction *I = &*inst;
        if(isa<CallInst>(&*inst) && inst->getType()->isVoidTy()) {
            auto call_inst = dyn_cast<CallInst>(&*inst);
            auto calledFun = call_inst->getCalledFunction();
            errs() << "!!!!!!!!!!!!!no of operands : " << (&*inst)->getNumOperands() << "\n";
            errs() << "!!!!!!!!!!!!!no of operands : " << *(&*inst) << "\n";
            assert(I->getNumOperands() > 2 && "Not enough opernads in verify/update checksum");
            bool usePayload = I->getName().contains("_with_payload");
            auto cksum = new Util::JsonObject();
            // auto arg = I->arg_begin() + 1;
            // auto arg_end = I->arg_end() - 1;
            std::vector<Value*> data;
            for(auto op = 1; op < I->getNumOperands()-2; op++) {
                data.push_back(I->getOperand(op));
            }
            cstring calcName = createCalculation("csum16", data,
                                                json->calculations, usePayload);
            cksum->emplace("name", genName("cksum_"));
            cksum->emplace("id", nextId("checksums"));
            // TODO(jafingerhut) - add line/col here?
            auto jleft = getFieldName(I->getOperand(I->getNumOperands()-2));
            cksum->emplace("target", jleft);
            cksum->emplace("type", "generic");
            cksum->emplace("calculation", calcName);
            auto bool_arg = I->getOperand(0);
            auto ifcond = new Util::JsonObject();
            if(auto bool_call = dyn_cast<CallInst>(bool_arg)) {
            errs() << "###########--- bool_arg : " << *I->getOperand(0) << "\n"
                    << "name is : " <<  bool_call->getCalledFunction()->getName() << "\n"; 
                if (bool_call->getCalledFunction()->getName() == "isValid" ||
                    bool_call->getCalledFunction()->getName() == "isInvalid") {
                        ifcond->emplace("type", "expression");
                        auto value = new Util::JsonObject();
                        value->emplace("op", "d2b");
                        value->emplace("left", Util::JsonValue::null);
                        value->emplace("right", getFieldName(dyn_cast<Instruction>(bool_arg)->getOperand(0)));
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
