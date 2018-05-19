// IIT Hyderabad
//
// authors:
// Venkata Keerthy, Pankaj K, Bhanu Prakash T, D Tharun Kumar
// {cs17mtech11018, cs15btech11029, cs15btech11037, cs15mtech11002}@iith.ac.in
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "ir/ir.h"
// #include "options.h"
#include "frontends/common/resolveReferences/referenceMap.h"
#include "frontends/p4/typeMap.h"

namespace LLVMIR
{

class MidEnd
{
    std::vector<DebugHook> hooks;

  public:
    P4::ReferenceMap refMap;
    P4::TypeMap typeMap;

    void addDebugHook(DebugHook hook) { hooks.push_back(hook); }
    const IR::ToplevelBlock *run(LLVMIROptions &options, const IR::P4Program *program);
};

} // namespace LLVMIR