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

#ifndef _BACKENDS_LLBMV2_DEPARSER_H_
#define _BACKENDS_LLBMV2_DEPARSER_H_

#include "lib/json.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/Support/raw_ostream.h"
#include "../JsonObjects.h"

using namespace llvm;
namespace LLBMV2 {

class ConvertDeparser {
    LLBMV2::JsonObjects* json;
 public:
    bool processDeparser(llvm::Function *F);
    explicit ConvertDeparser(LLBMV2::JsonObjects* json) :
        json(json){}
};

}  // namespace LLBMV2

#endif  /* _BACKENDS_LLBMV2_DEPARSER_H_ */
