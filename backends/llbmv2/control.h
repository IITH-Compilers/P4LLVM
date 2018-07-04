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

#ifndef _BACKENDS_BMV2_CONTROL_H_
#define _BACKENDS_BMV2_CONTROL_H_

#include "ir/ir.h"
#include "analyzer.h"
#include "frontends/p4/coreLibrary.h"
#include "frontends/p4/typeMap.h"
#include "frontends/p4/typeChecking/typeChecker.h"
#include "frontends/common/resolveReferences/referenceMap.h"
#include "midend/convertEnums.h"
#include "helpers.h"
#include "sharedActionSelectorCheck.h"
#include "simpleSwitch.h"
#include "toIR.h"
#include "frontends/p4/fromv1.0/v1model.h"


namespace LLBMV2 {

class ControlConverter : public Inspector {
    Backend*               backend;
    ToIR* toIR;    
    P4::ReferenceMap*      refMap;
    P4::TypeMap*           typeMap;
    
 public:
    bool preorder(const IR::PackageBlock* b) override;
    bool preorder(const IR::ControlBlock* b) override;
    bool preorder(const IR::P4Action* a) override;   

    explicit ControlConverter(Backend *backend) : backend(backend), toIR(new ToIR(this->backend)),
        refMap(backend->getRefMap()), typeMap(backend->getTypeMap())
    {setName("Control"); }
};

class ChecksumConverter : public Inspector {
    Backend* backend;
    ToIR* toIR;
    P4V1::V1Model& v1model;
   
    void convertChecksum(const IR::BlockStatement *block, bool verify);
    
 public:
    bool preorder(const IR::PackageBlock* b) override;
    bool preorder(const IR::ControlBlock* b) override;
    explicit ChecksumConverter(Backend *backend) : backend(backend), toIR(new ToIR(this->backend)), v1model(P4V1::V1Model::instance)
    { setName("UpdateChecksum"); }
};

class ConvertControl final : public PassManager {
 public:
    explicit ConvertControl(Backend *backend) {
        passes.push_back(new ControlConverter(backend));
        passes.push_back(new ChecksumConverter(backend));
        setName("ConvertControl");
    }
};

}  // namespace LLBMV2

#endif  /* _BACKENDS_BMV2_CONTROL_H_ */
