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

// #include "midend.h"
#include "frontends/p4/emitLLVMIR.h"
#include "frontends/p4/evaluator/evaluator.h"

namespace LLVMIR {

const IR::ToplevelBlock *MidEnd::run(LLVMIROptions &options, const IR::P4Program *program)
{
    if (program == nullptr)
        return nullptr;
    bool isv1 = options.langVersion == CompilerOptions::FrontendVersion::P4_14;
    if(isv1) {
        std::cout << "Error: We are not currently supporting P4_14 standard\n";
        exit(1);
    }
    auto evaluator = new P4::EvaluatorPass(&refMap, &typeMap);
    PassManager midEnd = {
        new P4::EmitLLVMIR(program, options.file, &refMap, &typeMap),
        evaluator,
    };

    midEnd.setName("LLVMIR-MidEnd");
    midEnd.addDebugHooks(hooks);
    program = program->apply(midEnd);
    if (::errorCount() > 0)
        return nullptr;

    return evaluator->getToplevelBlock();
}


} //End of namespace LLVMIR