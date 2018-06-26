#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/IR/Instructions.h"
#include <vector>
#include "redundantStoreElimination.h"

using namespace llvm;

namespace
{
struct RedundantStoreElimination : public FunctionPass
{
public:
    static char ID;
	RedundantStoreElimination() : FunctionPass(ID) {
	}

	~RedundantStoreElimination() {
	}

	virtual bool runOnFunction(Function &F) {
		errs() << F.getName() << " function hit in rse\n";
		std::vector<StoreInst*> sInst;
		for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
			if(auto store = dyn_cast<StoreInst>(&*I))	{
				store->dump();
				auto val = store->getValueOperand();
				auto ptr = store->getPointerOperand();
				bool flag = false;
				ptr->dump();
				errs() << "users of ptr -- \n";
				for(auto user_iter = ptr->user_begin(), E = ptr->user_end(); user_iter != E; ++user_iter){
					user_iter->dump();			
					if(dyn_cast<StoreInst>(*user_iter) == store) {
						errs() << "things do work\n";
						flag = true;
						continue;
					}
					if(flag) {
						errs() << "inside true flag\n";
						flag = false;
						if(auto st = dyn_cast<StoreInst>(*user_iter)) {
							errs() << "instruction to remove -- ";
							st->dump();
							errs() << "\n";
							sInst.push_back(st);
						}
					}
				}
			 errs()<<"----------------------------\n";
			}
		}
		for(auto st : sInst) {
			st->eraseFromParent();
		}
		return false;
	}

};
}

char RedundantStoreElimination::ID = 0;



extern "C" {
    FunctionPass *createStoreEliminationPass()
    {
    	return new RedundantStoreElimination();
    }
}