#include "llvm/IR/Module.h"

// namespace llvm {
extern "C" {
llvm::ModulePass *createJsonBackendPass(cstring);
// }
}