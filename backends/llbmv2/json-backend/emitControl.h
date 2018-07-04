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

#ifndef _BACKENDS_BMV2_CONTROL_H_
#define _BACKENDS_BMV2_CONTROL_H_

#include "lib/json.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/GlobalVariable.h"
#include "helpers.h"
#include "../JsonObjects.h"
#include <string>

namespace LLBMV2 {

class ControlConverter {
    LLBMV2::JsonObjects* json;
    std::map<llvm::BasicBlock*, cstring> condMap;
 
 Util::JsonObject* convertTable(llvm::CallInst *apply_call,
                            cstring cur_table_name,
                            cstring nex_table_name);
void convertConditional(llvm::BranchInst*, Util::JsonArray*);
cstring getApplyCallName(llvm::BasicBlock *bb);
 public:
    void processControl(llvm::Function* F);
    explicit ControlConverter(LLBMV2::JsonObjects* j) : json(j) {}
};

class ChecksumConverter {
    LLBMV2::JsonObjects* json;
 public:
    void processChecksum(llvm::Function* F);
    void processVerifyChecksum(llvm::Function* F);
    cstring createCalculation(cstring, std::vector<llvm::Value*>, Util::JsonArray*, bool);
    explicit ChecksumConverter(LLBMV2::JsonObjects* j) : json(j) {}
};

}  // namespace LLBMV2

#endif  /* _BACKENDS_BMV2_CONTROL_H_ */
