/*
IITH Compilers
authors: S Venkata Keerthy, D Tharun
email: {cs17mtech11018, cs15mtech11002}@iith.ac.in

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

#ifndef _BACKENDS_BMV2_PARSER_H_
#define _BACKENDS_BMV2_PARSER_H_

#include "ir/ir.h"
#include "lib/json.h"
#include "frontends/p4/typeMap.h"
#include "frontends/common/resolveReferences/referenceMap.h"
#include "backend.h"
#include "toIR.h"

namespace LLBMV2 {

class ParserConverter : public Inspector {
    Backend* backend;
    ToIR* toIR;
    P4::ReferenceMap*    refMap;
    P4::TypeMap*         typeMap;
    P4::P4CoreLibrary&   corelib;
    Value* llvmValue;    

 public:
    bool preorder(const IR::P4Parser* p) override;
    bool preorder(const IR::PackageBlock* b) override;
    bool preorder(const IR::ParserState* parserState) override;
    bool preorder(const IR::SelectExpression* t) override;
        
    
    explicit ParserConverter(Backend* backend) : backend(backend), toIR(new ToIR(this->backend)), refMap(backend->getRefMap()),
    typeMap(backend->getTypeMap()), corelib(P4::P4CoreLibrary::instance) {setName("ParserConverter"); }
};

class ConvertParser final : public PassManager {
 public:
    explicit ConvertParser(Backend* backend) {
        passes.push_back(new ParserConverter(backend));
        setName("ConvertParser");
    }
};

}  // namespace LLBMV2

#endif  /* _BACKENDS_BMV2_PARSER_H_ */
