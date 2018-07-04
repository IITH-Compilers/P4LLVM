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

#ifndef _BACKENDS_BMV2_SIMPLESWITCH_H_
#define _BACKENDS_BMV2_SIMPLESWITCH_H_

#include <algorithm>
#include <cstring>
#include "frontends/p4/fromv1.0/v1model.h"
#include "sharedActionSelectorCheck.h"

namespace LLBMV2 {

class Backend;

}  // namespace LLBMV2

namespace P4V1 {

class SimpleSwitch {
    LLBMV2::Backend* backend;
    V1Model&       v1model;


 protected:
    cstring convertHashAlgorithm(cstring algorithm);
    
 public:
    void modelError(const char* format, const IR::Node* place) const;
    void convertChecksum(const IR::BlockStatement* body, bool verify);

    void setPipelineControls(const IR::ToplevelBlock* blk, std::set<cstring>* controls,
                             std::map<cstring, cstring>* map);
    void setNonPipelineControls(const IR::ToplevelBlock* blk, std::set<cstring>* controls);
    void setUpdateChecksumControls(const IR::ToplevelBlock* blk, std::set<cstring>* controls);
    void setVerifyChecksumControls(const IR::ToplevelBlock* blk, std::set<cstring>* controls);
    void setDeparserControls(const IR::ToplevelBlock* blk, std::set<cstring>* controls);

    const IR::P4Control* getIngress(const IR::ToplevelBlock* blk);
    const IR::P4Control* getEgress(const IR::ToplevelBlock* blk);
    const IR::P4Parser*  getParser(const IR::ToplevelBlock* blk);

    explicit SimpleSwitch(LLBMV2::Backend* backend) :
        backend(backend), v1model(V1Model::instance)
    { CHECK_NULL(backend); }
};

}  // namespace P4V1

#endif /* _BACKENDS_BMV2_SIMPLESWITCH_H_ */
