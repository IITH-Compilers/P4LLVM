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

// midend passes

#include "midend/actionsInlining.h"
#include "midend/inlining.h"
#include "midend/removeReturns.h"
#include "midend/moveConstructors.h"
#include "midend/actionSynthesis.h"
#include "midend/localizeActions.h"
#include "midend/removeParameters.h"
#include "midend/local_copyprop.h"
#include "midend/simplifyKey.h"
#include "midend/simplifySelectCases.h"
#include "midend/simplifySelectList.h"
#include "midend/validateProperties.h"
#include "midend/eliminateTuples.h"
#include "midend/noMatch.h"
#include "midend/convertEnums.h"
#include "midend/midEndLast.h"
#include "midend/removeLeftSlices.h"

// frontend passes

#include "frontends/p4/uniqueNames.h"
#include "frontends/p4/moveDeclarations.h"
#include "frontends/p4/typeMap.h"
#include "frontends/p4/evaluator/evaluator.h"
#include "frontends/p4/typeChecking/typeChecker.h"
#include "frontends/common/resolveReferences/resolveReferences.h"
#include "frontends/p4/toP4/toP4.h"
#include "frontends/p4/simplify.h"
#include "frontends/p4/unusedDeclarations.h"
#include "frontends/common/constantFolding.h"
#include "frontends/p4/strengthReduction.h"
#include "frontends/p4/simplifyParsers.h"

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
        // new P4::Inline(&refMap, &typeMap, evaluator),
        new P4::InlineActions(&refMap, &typeMap),
        new P4::LocalizeAllActions(&refMap),
        new P4::UniqueNames(&refMap), // needed again after inlining
        new P4::UniqueParameters(&refMap, &typeMap),
        new P4::ClearTypeMap(&typeMap),
        new P4::SimplifyControlFlow(&refMap, &typeMap),
        new P4::RemoveActionParameters(&refMap, &typeMap),
        new P4::SimplifyKey(&refMap, &typeMap,
                            new P4::OrPolicy(
                                new P4::IsValid(&refMap, &typeMap),
                                new P4::IsLikeLeftValue())),
        new P4::RemoveExits(&refMap, &typeMap),
        new P4::ConstantFolding(&refMap, &typeMap),
        new P4::SimplifySelectCases(&refMap, &typeMap, false), // accept non-constant keysets
        new P4::HandleNoMatch(&refMap),
        new P4::SimplifyParsers(&refMap),
        new P4::StrengthReduction(),
        new P4::EliminateTuples(&refMap, &typeMap),
        new P4::LocalCopyPropagation(&refMap, &typeMap),
        new P4::SimplifySelectList(&refMap, &typeMap),
        new P4::MoveDeclarations(), // more may have been introduced
        new P4::SimplifyControlFlow(&refMap, &typeMap),
        new P4::ValidateTableProperties({"implementation"}),
        new P4::RemoveLeftSlices(&refMap, &typeMap),
        new P4::MidEndLast(),
        // LLVM IR Pass
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