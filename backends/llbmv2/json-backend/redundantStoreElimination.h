#include "llvm/IR/Function.h"

// namespace llvm {
extern "C" {
llvm::FunctionPass *createStoreEliminationPass();
// }
}