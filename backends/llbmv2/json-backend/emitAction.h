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

#ifndef _BACKENDS_BMV2_ACTION_H_
#define _BACKENDS_BMV2_ACTION_H_

// #include "ir/ir.h"
// #include "backend.h"
// #include "simpleSwitch.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstIterator.h"
#include "../JsonObjects.h"
#include "helpers.h"
#include "emitParser.h"

namespace LLBMV2 {

class ConvertActions {
    // Backend*               backend;
    // P4::ReferenceMap*      refMap;
    // P4::TypeMap*           typeMap;
    LLBMV2::JsonObjects*     json;
    // ExpressionConverter*   conv;

    void convertActionBody(llvm::Function *F, Util::JsonArray *body);
    void convertActionParams(llvm::Function *F, Util::JsonArray* params);
    Util::IJson* getJsonExp(llvm::Value *inst);
    // bool preorder(const IR::PackageBlock* package);
 public:
    void processActions(llvm::Function *F);
    explicit ConvertActions(LLBMV2::JsonObjects *json) : json(json) {
    }
    ~ConvertActions() {}
};

}  // namespace BMV2

#endif  /* _BACKENDS_BMV2_ACTION_H_ */
