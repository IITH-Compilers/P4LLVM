#ifndef _FRONTENDS_P4_EMITLLVMIR_H_
#define _FRONTENDS_P4_EMITLLVMIR_H_

#include "ir/ir.h"
#include "ir/visitor.h"
#include "frontends/common/resolveReferences/referenceMap.h"
#include "frontends/p4/typeChecking/typeChecker.h"

#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/InitializePasses.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Scalar.h"

#include "frontends/p4/methodInstance.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <iterator>
#include <memory>
#include <string>
#include <vector>
#include "iostream"
#include "lib/nullstream.h"
#include "llvm/ADT/APInt.h"

using namespace llvm;

#define VERBOSE 1

#if VERBOSE
#ifndef MYDEBUG
    #define MYDEBUG(x) x
#endif
#else 
    #define MYDEBUG(x) 
#endif

#endif /* _FRONTENDS_P4_LLVMIR_H_ */
