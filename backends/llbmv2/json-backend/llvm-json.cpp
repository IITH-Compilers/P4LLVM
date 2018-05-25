#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
using namespace llvm;

namespace
{
struct JsonBackend : public ModulePass
{
    static char ID;
    JsonBackend() : ModulePass(ID) {}

    virtual bool runOnModule(Module &M)
    {
        errs() << "I saw a Module called " << M.getName() << "!\n";
        return false;
    }
};
}

char JsonBackend::ID = 0;

// llvm::ModulePass *createJsonBackendPass()
extern "C" {
    ModulePass *createJsonBackendPass()
    {
    return new JsonBackend();
    }
}