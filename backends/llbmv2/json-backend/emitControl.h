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

#ifndef _BACKENDS_BMV2_CONTROL_H_
#define _BACKENDS_BMV2_CONTROL_H_

// #include "ir/ir.h"
#include "lib/json.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstIterator.h"
#include "helpers.h"
#include "../JsonObjects.h"

namespace LLBMV2 {

// class ControlConverter {
//     // Backend*               backend;
//     // P4::ReferenceMap*      refMap;
//     // P4::TypeMap*           typeMap;
//     // ExpressionConverter*   conv;
//     BMV2::JsonObjects*     json;

//  protected:
//     Util::IJson* convertTable(const CFG::TableNode* node,
//                               Util::JsonArray* action_profiles,
//                               BMV2::SharedActionSelectorCheck& selector_check);
//     void convertTableEntries(const IR::P4Table *table, Util::JsonObject *jsonTable);
//     cstring getKeyMatchType(const IR::KeyElement *ke);
//     /// Return 'true' if the table is 'simple'
//     bool handleTableImplementation(const IR::Property* implementation, const IR::Key* key,
//                                    Util::JsonObject* table, Util::JsonArray* action_profiles,
//                                    BMV2::SharedActionSelectorCheck& selector_check);
//     Util::IJson* convertIf(const CFG::IfNode* node, cstring prefix);
//     Util::IJson* convertControl(const IR::ControlBlock* block, cstring name,
//                                 Util::JsonArray *counters, Util::JsonArray* meters,
//                                 Util::JsonArray* registers);

//  public:
//     bool preorder(const IR::PackageBlock* b) override;
//     bool preorder(const IR::ControlBlock* b) override;

//     explicit ControlConverter(Backend *backend) : backend(backend),
//         refMap(backend->getRefMap()), typeMap(backend->getTypeMap()),
//         conv(backend->getExpressionConverter()), json(backend->json)
//     { setName("Control"); }
// };

class ChecksumConverter {
    // Backend* backend;
    LLBMV2::JsonObjects* json;
 public:
    void processChecksum(llvm::Function* F);
    void processVerifyChecksum(llvm::Function* F);
    cstring createCalculation(cstring, std::vector<llvm::Value*>, Util::JsonArray*, bool);
    explicit ChecksumConverter(LLBMV2::JsonObjects* j) : json(j) {}
};

// class ConvertControl final : public PassManager {
//  public:
//     explicit ConvertControl(Backend *backend) {
//         passes.push_back(new ControlConverter(backend));
//         passes.push_back(new ChecksumConverter(backend));
//         setName("ConvertControl");
//     }
// };

}  // namespace BMV2

#endif  /* _BACKENDS_BMV2_CONTROL_H_ */
