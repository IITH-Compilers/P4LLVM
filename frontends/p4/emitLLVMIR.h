
#ifndef _FRONTENDS_P4_EMITLLVMIR_H_
#define _FRONTENDS_P4_EMITLLVMIR_H_

#include "ir/ir.h"
#include "frontends/common/resolveReferences/referenceMap.h"
#include "frontends/p4/typeChecking/typeChecker.h"

namespace P4 {

class EmitLLVMIR : public Inspector {

 public:
    EmitLLVMIR(ReferenceMap* refMap, TypeMap* typeMap) {
        CHECK_NULL(refMap); CHECK_NULL(typeMap);
        setName("EmitLLVMIR");
    }
    

};

}  // namespace P4

#endif /* _FRONTENDS_P4_LLVMIR_H_ */
