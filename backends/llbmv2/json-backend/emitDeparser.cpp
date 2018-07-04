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

#include "emitDeparser.h"
#include "helpers.h"

using namespace LLVMJsonBackend;

namespace LLBMV2 {

bool ConvertDeparser::processDeparser(Function *F) {

    auto result = new Util::JsonObject();
    result->emplace("name", "deparser"); // at least in simple_router this name is hardwired
    result->emplace("id", nextId("deparser"));
    result->emplace("source_info", Util::JsonValue::null);
    auto order = mkArrayField(result, "order");

    for(auto inst = inst_begin(F); inst != inst_end(F); inst++) {
        if(isa<CallInst>(&*inst)) {
            auto emit_call = dyn_cast<CallInst>(&*inst);
            auto funName = emit_call->getCalledFunction()->getName();
            if(funName.contains("emit")) {
                int argCount = emit_call->getFunctionType()->getFunctionNumParams();
                assert(argCount == 1 &&
                       "More than one argument in emit function, not expected");
                auto headerName = getFieldName(emit_call->getOperand(0)).substr(1);
                order->append(headerName);
            }
            else {
                errs() << "ERROR : Deparser contains unknown function call\n"
                        << *inst << "\n";
                exit(1);
            }
        }
    }
    json->deparsers->append(result);
    return false;
}

}  // namespace LLBMV2
