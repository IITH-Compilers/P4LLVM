/*
IIT Hyderabad

authors:
Venkata Keerthy, Pankaj K, Bhanu Prakash T, D Tharun Kumar
{cs17mtech11018, cs15btech11029, cs15btech11037, cs15mtech11002}@iith.ac.in

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

#include <getopt.h>
#include "frontends/common/options.h"

namespace LLVMIR {

// enum class Target { UNKNOWN, PORTABLE, SIMPLE };

class LLVMIROptions : public CompilerOptions {
 public:
    // LLVMIR::Target arch = LLVMIR::Target::UNKNOWN;
    LLVMIROptions() {
        langVersion = CompilerOptions::FrontendVersion::P4_16;
        // registerOption("--arch", "arch",
        //                [this](const char* arg) {
        //                 if (!strcmp(arg, "psa")) {
        //                     arch = LLVMIR::Target::PORTABLE;
        //                 } else if (!strcmp(arg, "ss")) {
        //                     arch = LLVMIR::Target::SIMPLE;
        //                 } else {
        //                     ::error("Unknown architecture %1%", arg);
        //                 }
        //                 return true; },
        //                "Compile for the specified architecture (psa or ss), default is ss.");
    }
};

using LLVMIRContext = P4CContextWithOptions<LLVMIROptions>;

}  // namespace LLVMIR