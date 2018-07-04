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

#include <stdio.h>
#include <string>
#include <iostream>

#include "ir/ir.h"
#include "control-plane/p4RuntimeSerializer.h"
#include "frontends/common/applyOptionsPragmas.h"
#include "frontends/common/parseInput.h"
#include "frontends/p4/frontend.h"
#include "lib/error.h"
#include "lib/exceptions.h"
#include "lib/gc.h"
#include "lib/log.h"
#include "lib/nullstream.h"
#include "backend.h"
#include "midend.h"
#include "options.h"

// static void log_dump1(const IR::Node *node, const char *head) {
//     if (node) {
//         if (head)
//             std::cout << '+' << std::setw(strlen(head)+6) << std::setfill('-') << "+\n| "
//                       << head << " |\n" << '+' << std::setw(strlen(head)+3) << "+" <<
//                       std::endl << std::setfill(' ');
//         // if (LOGGING(2))
//             // dump(node);
//         // else
//             std::cout << *node << std::endl; }
// }

int main(int argc, char *const argv[]) {
    setup_gc_logging();

    AutoCompileContext autoBMV2Context(new LLBMV2::BMV2Context);
    auto& options = LLBMV2::BMV2Context::get().options();
    options.langVersion = LLBMV2::BMV2Options::FrontendVersion::P4_16;
    options.compilerVersion = "0.0.5";

    if (options.process(argc, argv) != nullptr)
        options.setInputFile();
    if (::errorCount() > 0)
        return 1;

    auto hook = options.getDebugHook();

    // BMV2 is required for compatibility with the previous compiler.
    options.preprocessor_options += " -D__TARGET_BMV2__";
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

    const IR::ToplevelBlock* toplevel = nullptr;
    LLBMV2::MidEnd midEnd(options);
    midEnd.addDebugHook(hook);
    try {
        toplevel = midEnd.process(program);
        if (::errorCount() > 1 || toplevel == nullptr ||
            toplevel->getMain() == nullptr)
            return 1;
        if (options.dumpJsonFile)
            JSONGenerator(*openFile(options.dumpJsonFile, true)) << program << std::endl;
    } catch (const Util::P4CExceptionBase &bug) {
        std::cerr << bug.what() << std::endl;
        return 1;
    }
    if (::errorCount() > 0)
        return 1;

    // backend depends on the modified refMap and typeMap from midEnd.
    LLBMV2::Backend backend(options.isv1(), &midEnd.refMap,
            &midEnd.typeMap, &midEnd.enumMap, options.file);
    try {
        backend.addDebugHook(hook);
        backend.process(toplevel, options);
        backend.convert();
        backend.addMetaData();
        backend.dumpLLVMIR();
        backend.runLLVMPasses(options);
        // log_dump1(toplevel,"From backend of bmv2");
    } catch (const Util::P4CExceptionBase &bug) {
        std::cerr << bug.what() << std::endl;
        return 1;
    }
    if (::errorCount() > 0)
        return 1;

    P4::serializeP4RuntimeIfRequired(program, options);

    return ::errorCount() > 0;
}
