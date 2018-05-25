#include "llvm/IR/Module.h"

// namespace llvm {
extern "C" {
    ModulePass *createJsonBackendPass();
// }
}