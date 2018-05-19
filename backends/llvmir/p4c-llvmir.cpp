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

#include <stdio.h>
#include <string>
#include <iostream>
#include "frontends/common/applyOptionsPragmas.h"
#include "frontends/common/parseInput.h"
#include "frontends/p4/frontend.h"
#include "LLVMIROptions.h"

#include "ir/ir.h"
#include "lib/log.h"
#include "lib/crash.h"
#include "lib/exceptions.h"
#include "lib/gc.h"
#include "lib/nullstream.h"

#include "midend.h"

int main(int argc, char *const argv[])
{
  setup_gc_logging();
  AutoCompileContext autoLLVMIRContext(new LLVMIR::LLVMIRContext);
  auto& options = LLVMIR::LLVMIRContext::get().options();
  options.langVersion = LLVMIR::LLVMIROptions::FrontendVersion::P4_16;
  options.compilerVersion = "0.0.0";

  if (options.process(argc, argv) != nullptr)
      options.setInputFile();
  if (::errorCount() > 0)
      exit(1);

// Front-end: Parsing
  auto hook = options.getDebugHook();
  auto program = P4::parseP4File(options);
  if (program == nullptr || ::errorCount() > 0)
      return 1;
  try {
      P4::P4COptionPragmaParser optionsPragmaParser;
      program->apply(P4::ApplyOptionsPragmas(optionsPragmaParser));

      P4::FrontEnd frontend;
      frontend.addDebugHook(hook);
      program = frontend.run(options, program);
  } catch (const Util::P4CExceptionBase &bug) {
      std::cerr << bug.what() << std::endl;
      return 1;
  }
  if (program == nullptr || ::errorCount() > 0)
      return 1;
  // Mid-end and Backend
  LLVMIR::MidEnd midend;
  midend.addDebugHook(hook);
  auto toplevel = midend.run(options, program);
  // if (options.dumpJsonFile)
  //     JSONGenerator(*openFile(options.dumpJsonFile, true)) << program << std::endl;
  if (::errorCount() > 0)
      return 1; 
  if(toplevel != nullptr)
      std::cout << "Success\n";
  // PassManager starts here: 
  return 0;
}