/*
IITH Compilers
authors: S Venkata Keerthy, D Tharun
email: {cs15mtech11002, cs17mtech11018}@iith.ac.in

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
		std::vector<StoreInst*> sInst;
		for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
			if(auto store = dyn_cast<StoreInst>(&*I))	{
				auto val = store->getValueOperand();
				auto ptr = store->getPointerOperand();
				bool flag = false;
				for(auto user_iter = ptr->user_begin(), E = ptr->user_end(); user_iter != E; ++user_iter){	
					if(dyn_cast<StoreInst>(*user_iter) == store) {
						flag = true;
						continue;
					}
					if(flag) {
						flag = false;
						if(auto st = dyn_cast<StoreInst>(*user_iter)) {
							sInst.push_back(st);
						}
					}
				}
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