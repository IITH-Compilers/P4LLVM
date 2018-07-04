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

#ifndef _BACKENDS_BMV2_ACTION_H_
#define _BACKENDS_BMV2_ACTION_H_

#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstIterator.h"
#include "../JsonObjects.h"
#include "helpers.h"
#include "emitParser.h"

namespace LLBMV2 {

class ConvertActions {
    LLBMV2::JsonObjects*     json;
    std::map<cstring, unsigned> actionParamMap;
    std::vector<cstring> actionParamList;

    void convertActionBody(llvm::Function *F, Util::JsonArray *body);
    void convertActionParams(llvm::Function *F, Util::JsonArray* params);
    // Util::IJson* getJsonExp(llvm::Value *inst);
    bool isAssignment(llvm::StoreInst*);
    unsigned getRuntimeID(cstring paramName) {
        if(actionParamMap.find(paramName) == actionParamMap.end())
            assert(false && "Param not found");
        return actionParamMap[paramName];
    }
    void setRuntimeID(cstring paramName, unsigned id) {
        if(actionParamMap.find(paramName) == actionParamMap.end())
            actionParamMap[paramName] = id;
    }
    void addToActionParamList(cstring paramName) {
        if (std::find(actionParamList.begin(), actionParamList.end(), paramName)
            == actionParamList.end())
            actionParamList.push_back(paramName);
        llvm::errs() << "Action Param listed: " << paramName << "\n";
    }
    bool isActionParam(cstring paramName) {
        llvm::errs() << "Action param asked for: " << paramName << "\n";
        if (std::find(actionParamList.begin(), actionParamList.end(), paramName)
            != actionParamList.end())
            return true;
        return false;
    }
 public:
    void processActions(llvm::Function *F);
    explicit ConvertActions(LLBMV2::JsonObjects *json) : json(json) {
    }
    ~ConvertActions() {}
};

}  // namespace BMV2

#endif  /* _BACKENDS_BMV2_ACTION_H_ */
