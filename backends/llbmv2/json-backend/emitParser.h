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

#ifndef _BACKENDS_LLBMV2_PARSER_H_
#define _BACKENDS_LLBMV2_PARSER_H_

// #include "ir/ir.h"
#include "lib/json.h"
// #include "frontends/p4/typeMap.h"
// #include "frontends/common/resolveReferences/referenceMap.h"
// #include "backend.h"
// #include "expression.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"

namespace LLBMV2 {

class JsonObjects;

class ParserConverter {
    // Backend* backend;
    // P4::ReferenceMap*    refMap;
    // P4::TypeMap*         typeMap;
    LLBMV2::JsonObjects*   json;
    // ExpressionConverter* conv;
    // P4::P4CoreLibrary&   corelib;
    // std::map<const IR::P4Parser*, Util::IJson*> parser_map;
    // std::map<const IR::ParserState*, Util::IJson*> state_map;
    // std::vector<Util::IJson*> context;

 protected:
    // void convertSimpleKey(const IR::Expression* keySet, mpz_class& value, mpz_class& mask) const;
    // unsigned combine(const IR::Expression* keySet, const IR::ListExpression* select,
    //                  mpz_class& value, mpz_class& mask, bool& is_vset, cstring& vset_name) const;
    // Util::IJson* stateName(IR::ID state);
    // Util::IJson* toJson(const IR::P4Parser* cont);
    // Util::IJson* toJson(const IR::ParserState* state);
    // Util::IJson* convertSelectKey(const IR::SelectExpression* expr);
    // Util::IJson* convertPathExpression(const IR::PathExpression* expr);
    // Util::IJson* createDefaultTransition();
    // std::vector<Util::IJson*> convertSelectExpression(const IR::SelectExpression* expr);

 public:
    // ParserConverter(LLBMV2::JsonObjects *json):json(json);
    Util::IJson* convertParserStatement(llvm::Instruction*);
    bool processParser(llvm::Function *F);
    bool isIndex1D(llvm::GetElementPtrInst *gep);
    std::string getMultiDimFieldName(llvm::GetElementPtrInst *gep);
    unsigned get1DIndex(llvm::GetElementPtrInst *gep);
    std::string getFieldName(llvm::Value*);
    // bool preorder(const IR::PackageBlock* b) override;
    explicit ParserConverter(LLBMV2::JsonObjects* json) : json(json){}
    // corelib(P4::P4CoreLibrary::instance) {}
};

// class ConvertParser final : public PassManager {
//  public:
//     explicit ConvertParser(Backend* backend) {
//         passes.push_back(new ParserConverter(backend));
//         setName("ConvertParser");
//     }
// };

}  // namespace LLBMV2

#endif  /* _BACKENDS_LLBMV2_PARSER_H_ */
