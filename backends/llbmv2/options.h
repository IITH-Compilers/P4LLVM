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

#ifndef _BACKENDS_BMV2_OPTIONS_H_
#define _BACKENDS_BMV2_OPTIONS_H_

#include <getopt.h>
#include "frontends/common/options.h"

namespace LLBMV2 {

enum class Target { UNKNOWN, PORTABLE, SIMPLE };

class BMV2Options : public CompilerOptions {
 public:
    LLBMV2::Target arch = LLBMV2::Target::UNKNOWN;
    const char* bpath = "./backends/llbmv2/json-backend/libLLVMJsonBackend.so";
    BMV2Options()
    {
        registerOption("--arch", "arch",
                       [this](const char* arg) {
                        if (!strcmp(arg, "psa")) {
                            arch = LLBMV2::Target::PORTABLE;
                        } else if (!strcmp(arg, "ss")) {
                            arch = LLBMV2::Target::SIMPLE;
                        } else {
                            ::error("Unknown architecture %1%", arg);
                        }
                        return true; },
                       "Compile for the specified architecture (psa or ss), default is ss.");
        registerOption("--bpath", "bpath",
                       [this](const char *arg) {
                           bpath = arg;
                           return true;
                       },
                       "provide path to libLLVMJsonBackend.so (default: build/backends/llbmv2/json-backend/libLLVMJsonBackend.so)");
     }
};

using BMV2Context = P4CContextWithOptions<BMV2Options>;

}  // namespace LLBMV2

#endif  /* _BACKENDS_BMV2_OPTIONS_H_ */
