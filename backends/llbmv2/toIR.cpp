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

#include "toIR.h"

Value* ToIR::createExternFunction(int no, const IR::MethodCallExpression* mce, cstring name, MethodInstance* minst)   {
    std::vector<Type*> args;
    std::vector<Value*> param;

    auto params = minst->getOriginalParameters()->parameters;

    for(int i=0; i<no; i++){
        if(auto inst = mce->arguments->at(i)->to<IR::ListExpression>())   {
            for(auto c : inst->components) {
                if(params.at(i)->hasOut())
                    llvmValue = processExpression(c,nullptr,nullptr,true);
                else
                    llvmValue = processExpression(c);
                assert(llvmValue != nullptr && "processExpression should not return null");
                args.push_back(llvmValue->getType());
                param.push_back(llvmValue);
                llvmValue = nullptr;
            }
        }
        else {
            if(params.at(i)->hasOut())
                llvmValue = processExpression(mce->arguments->at(i),nullptr,nullptr,true);
            else
                llvmValue = processExpression(mce->arguments->at(i));
            assert(llvmValue != nullptr && "processExpression should not return null");
            args.push_back(llvmValue->getType());
            param.push_back(llvmValue);
            llvmValue = nullptr;
        } 
    }

    FunctionType *FT = FunctionType::get(backend->Builder.getVoidTy(), args, false);
    Function *decl = Function::Create(FT, Function::ExternalLinkage,  name.c_str(), backend->TheModule.get());
    return backend->Builder.CreateCall(decl, param);
}

Value* ToIR::processExpression(const IR::Expression* e, BasicBlock* bbIf/*=nullptr*/, BasicBlock* bbElse/*=nullptr*/, bool required_alloca /*=false*/) {

    assert(e != nullptr);
    if(e->is<IR::Operation_Unary>())    {
        const IR::Operation_Unary* oue = e->to<IR::Operation_Unary>();
        Value* exp = processExpression(oue->expr, bbIf, bbElse);
        if(e->is<IR::Cmpl>()) 
            return backend->Builder.CreateXor(exp,-1);

        if(e->is<IR::Neg>())    
            return backend->Builder.CreateSub(ConstantInt::get(exp->getType(),0), exp);

        if(e->is<IR::LNot>())
            return backend->Builder.CreateICmpEQ(exp, ConstantInt::get(exp->getType(),0));

        if(auto c = e->to<IR::Cast>())  {
            int srcWidth = oue->expr->type->width_bits();
            int destWidth = c->destType->width_bits();
            if(srcWidth < destWidth)   {
                if(auto type = c->destType->to<IR::Type_Bits>())    {
                    if(type->isSigned)
                        return backend->Builder.CreateSExt(exp,backend->getType(c->destType));
                    else
                        return backend->Builder.CreateZExt(exp,backend->getType(c->destType));                        
                }
                return backend->Builder.CreateZExtOrBitCast (exp,backend->getType(c->destType));
            }
            else  
                return backend->Builder.CreateTruncOrBitCast(exp,backend->getType(c->destType));
        }

        if (e->is<IR::Member>()) {
            const IR::Member* expression = e->to<IR::Member>();        
            auto type = backend->getTypeMap()->getType(expression, true);
            if (type->is<IR::Type_Error>() && expression->expr->is<IR::TypeNameExpression>()) {
                // this deals with constants that have type 'error'
                auto decl = type->to<IR::Type_Error>()->getDeclByName(expression->member.name);
                ErrorCodesMap errCodes = backend->getErrorCodesMap();
                auto errorValue = errCodes.at(decl);
                return ConstantInt::get(backend->Builder.getInt32Ty(),errorValue);        
            }
            if (type->is<IR::Type_Enum>() && expression->expr->is<IR::TypeNameExpression>()) {
                auto decl = type->to<IR::Type_Enum>()->getDeclByName(expression->member.name)->getName();
                auto it = backend->enums.find(type->to<IR::Type_Enum>()->name);  
                if(it != backend->enums.end()) {
                    auto itr = std::find(it->second.begin(), it->second.end(), decl);
                    auto enumValue = std::distance(it->second.begin(), itr)+1;
                    return ConstantInt::get(backend->Builder.getInt32Ty(),enumValue);                     
                }
                else {
                    std::vector<cstring> enumContainer; 
                    for (auto e : *(type->to<IR::Type_Enum>())->getDeclarations()) {
                        enumContainer.push_back(e->getName().name);
                    }   
                    auto itr = std::find(enumContainer.begin(), enumContainer.end(), decl);                    
                    auto enumValue = std::distance(enumContainer.begin(), itr)+1;                    
                    backend->enums.insert(make_pair(type->to<IR::Type_Enum>()->name,enumContainer));
                    return ConstantInt::get(backend->Builder.getInt32Ty(),enumValue);                                         
                }
            }
            Value *ex = processExpression(e->to<IR::Member>()->expr, nullptr, nullptr, true);
            int ext_i = backend->structIndexMap[((PointerType *)ex->getType())->getElementType()][std::string(e->to<IR::Member>()->member.name)];
            std::vector<Value *> idx;
            idx.push_back(ConstantInt::get(backend->TheContext, llvm::APInt(32, 0, false)));
            idx.push_back(ConstantInt::get(backend->TheContext, llvm::APInt(32, ext_i, false)));
            ex = backend->Builder.CreateGEP(ex, idx);
            if(ex->getType()->getPointerElementType()->isVectorTy()) {
                auto width = dyn_cast<SequentialType>(ex->getType()->getPointerElementType())->getNumElements();
                auto x = Type::getIntNPtrTy(backend->TheContext, width);
                auto tmp = backend->Builder.CreateBitCast(ex, x);
                ex = tmp;
            }
            if (required_alloca) 
                return ex;
            else {
        		return backend->Builder.CreateLoad(ex);
            }
        }
        else
            BUG("unhandled op_unary");
    }
    if(e->is<IR::BoolLiteral>())    {
        const IR::BoolLiteral* c = e->to<IR::BoolLiteral>();            
        return ConstantInt::get(backend->getType(typemap->getType(c)),c->value);            
    }

    if(e->is<IR::Constant>()) {  
        const IR::Constant* c = e->to<IR::Constant>();
        auto ty = typemap->getType(c);
        if(!ty)
            ty = c->type;
        return ConstantInt::get(backend->getType(ty),(c->value).get_ui());
    }


    if(e->is<IR::PathExpression>()) {
        cstring name = refmap->getDeclaration(e->to<IR::PathExpression>()->path)->getName();    
        Value* v = backend->st.lookupGlobal("alloca_"+name);
        assert(v != nullptr);
        if(required_alloca)
            return v;
        else   {
            auto a = backend->Builder.CreateLoad(v);
            return a;
        }
    }


    if(e->is<IR::Operation_Binary>())   {
        const IR::Operation_Binary* obe = e->to<IR::Operation_Binary>();
        Value* left; Value* right;
        left = processExpression(obe->left, bbIf, bbElse);
        
        if(!(e->is<IR::LAnd>() || e->is<IR::LOr>()))    {
            right = processExpression(obe->right, bbIf, bbElse);
        }

        int leftWidth = obe->left->type->width_bits();
        int rightWidth = obe->right->type->width_bits();
        if(leftWidth < rightWidth) //extend left to fit right
        {       
            if(auto type = obe->right->type->to<IR::Type_Bits>())    {
                if(type->isSigned)
                    left = backend->Builder.CreateSExt(left,backend->getType(obe->right->type));
                else
                    left = backend->Builder.CreateZExt(left,backend->getType(obe->right->type));                        
            }
            else
                left = backend->Builder.CreateZExtOrBitCast (left,backend->getType(obe->right->type));
        }
        else if (leftWidth > rightWidth) //extend right to fit left
        {   
            if(auto type = obe->right->type->to<IR::Type_Bits>())    {
                if(type->isSigned)
                    right = backend->Builder.CreateSExt(right,backend->getType(obe->left->type));
                else
                    right = backend->Builder.CreateZExt(right,backend->getType(obe->left->type));                        
            }
            else
                right = backend->Builder.CreateZExtOrBitCast (right,backend->getType(obe->left->type));
        }

        if(e->is<IR::Add>())    
            return backend->Builder.CreateAdd(left,right);   

        if(e->is<IR::Sub>())    
            return backend->Builder.CreateSub(left,right);   

        if(e->is<IR::Mul>())    
            return backend->Builder.CreateMul(left,right);   

        if(e->is<IR::Div>())    
            return backend->Builder.CreateSDiv(left,right);  

        if(e->is<IR::Mod>())
            return backend->Builder.CreateSRem(left,right);  

        if(e->is<IR::Shl>())
            return backend->Builder.CreateShl(left,right);   

        if(e->is<IR::Shr>())
            return backend->Builder.CreateLShr(left,right);

        if(e->is<IR::BAnd>())
            return backend->Builder.CreateAnd(left,right);
        
        if(e->is<IR::BOr>())
            return backend->Builder.CreateOr(left,right);

        if(e->is<IR::BXor>())
            return backend->Builder.CreateXor(left,right);

        if(e->is<IR::LAnd>())   {
            BasicBlock* bbNext = BasicBlock::Create(backend->TheContext, "land.rhs", backend->function);                              
            BasicBlock* bbEnd;
            
            Value* icmp1;
            Value* icmp2;

            if(obe->left->is<IR::PathExpression>()) {
                icmp1 = backend->Builder.CreateICmpNE(left,ConstantInt::get(left->getType(),0));
                bbEnd = BasicBlock::Create(backend->TheContext, "land.end", backend->function);
                backend->Builder.CreateCondBr(icmp1, bbNext, bbEnd);             
            }
            else    {
                assert(bbElse != nullptr);
                icmp1 = left;
                backend->Builder.CreateCondBr(icmp1, bbNext, bbElse);
            }

            backend->Builder.SetInsertPoint(bbNext);
            BasicBlock* bbParent = bbNext->getSinglePredecessor();
            assert(bbParent != nullptr);
            
            right = processExpression(obe->right, bbIf, bbElse);
            if(obe->right->is<IR::PathExpression>())    {
                icmp2 = backend->Builder.CreateICmpNE(right,ConstantInt::get(right->getType(),0));   
                backend->Builder.CreateBr(bbEnd);
            }
            else    {
                icmp2 = right;
                return icmp2;       
            }

            backend->Builder.SetInsertPoint(bbEnd);
            PHINode* phi = backend->Builder.CreatePHI(icmp1->getType(), 2);
            phi->addIncoming(ConstantInt::getFalse(backend->TheContext), bbParent);
            phi->addIncoming(icmp2, bbNext);
            return phi;
        }

        if(e->is<IR::LOr>())    {
            BasicBlock* bbTrue;
            BasicBlock* bbFalse = BasicBlock::Create(backend->TheContext, "land.rhs", backend->function);
            
            Value* icmp1;
            Value* icmp2;
            
            if(obe->left->is<IR::PathExpression>()) {
                bbTrue = BasicBlock::Create(backend->TheContext, "land.end", backend->function);
                icmp1 = backend->Builder.CreateICmpNE(left,ConstantInt::get(left->getType(),0));
                backend->Builder.CreateCondBr(icmp1, bbTrue, bbFalse);
            }
            else    {
                assert(bbIf != nullptr);
                icmp1 = left;
                backend->Builder.CreateCondBr(icmp1, bbIf, bbFalse);
            }

            backend->Builder.SetInsertPoint(bbFalse);
            BasicBlock* bbParent = bbFalse->getSinglePredecessor();
            assert(bbParent != nullptr);
            
            right = processExpression(obe->right, bbIf, bbElse);
            if(obe->right->is<IR::PathExpression>())    {               
                icmp2 = backend->Builder.CreateICmpNE(right,ConstantInt::get(right->getType(),0));
                backend->Builder.CreateBr(bbTrue);
            }
            else    {
                icmp2 = right;
                return icmp2;
            }

            backend->Builder.SetInsertPoint(bbTrue);
            PHINode* phi = backend->Builder.CreatePHI(icmp1->getType(), 2);
            phi->addIncoming(ConstantInt::getTrue(backend->TheContext), bbParent);
            phi->addIncoming(icmp2, bbFalse);
            return phi;
        }

        if(e->is<IR::Operation_Relation>()) {
            if(obe->right->is<IR::Constant>())
                right = processExpression(obe->right, bbIf, bbElse);
            
            if(e->is<IR::Equ>())
                return backend->Builder.CreateICmpEQ(left,right);

            if(e->is<IR::Neq>())    
                return backend->Builder.CreateICmpNE(left,right);
                
            if(e->is<IR::Lss>())    
                return backend->Builder.CreateICmpSLT(left,right);
            
            if(e->is<IR::Leq>())    
                return backend->Builder.CreateICmpSLE(left,right);

            if(e->is<IR::Grt>())    
                return backend->Builder.CreateICmpSGT(left,right);

            if(e->is<IR::Geq>())    
                return backend->Builder.CreateICmpSGE(left,right);
        }
            
    }

    if(e->is<IR::Operation_Ternary>())    {
        const IR::Operation_Ternary* ote = e->to<IR::Operation_Ternary>();
        Value* e0 = processExpression(ote->e0);
        Value* e1 = processExpression(ote->e1);
        Value* e2 = processExpression(ote->e2);

        if(e->is<IR::Mux>())    {
            return backend->Builder.CreateSelect(e0, e1, e2);
        }

    }

    if(e->is<IR::MethodCallExpression>())    {
        const IR::MethodCallExpression* mce = e->to<IR::MethodCallExpression>();           

        auto minst = P4::MethodInstance::resolve(mce, refmap, typemap);
        if (minst->is<P4::ExternMethod>()) {
            auto extmeth = minst->to<P4::ExternMethod>();

            if (extmeth->originalExternType->name == corelib.packetIn.name &&
                extmeth->method->name == corelib.packetIn.lookahead.name) {
                BUG_CHECK(mce->typeArguments->size() == 1,
                        "Expected 1 type parameter for %1%", extmeth->method);
                auto targ = mce->typeArguments->at(0);
                auto typearg = backend->getTypeMap()->getTypeType(targ, true);
                int width = typearg->width_bits();
                BUG_CHECK(width > 0, "%1%: unknown width", targ);

                std::vector<Type*> args;
                FunctionType *FT = FunctionType::get(backend->getType(typearg), args, false);
                Function *function = Function::Create(FT, Function::ExternalLinkage,  
                                            extmeth->method->name.name.c_str(), backend->TheModule.get());
                return backend->Builder.CreateCall(function);    
            }
        
            if (extmeth->method->name.name == corelib.packetIn.extract.name) {
                int argCount = mce->arguments->size();
                if (argCount == 1 || argCount == 2) {
                    auto arg = mce->arguments->at(0);
                    auto argtype = typemap->getType(arg, true);
                    if (!argtype->is<IR::Type_Header>()) {
                        ::error("%1%: extract only accepts arguments with header types, not %2%",
                                arg, argtype);
                        return nullptr;
                    }
       
                    if (argCount == 1) {
                        // std::cout << "calling processexp from mcs of convert parser stmt: ac=1\n";
                        return createExternFunction(1,mce,extmeth->method->name.name,minst);                      
                    }

                    if (argCount == 2) {
                        createExternFunction(2,mce,extmeth->method->name.name,minst);
                        return nullptr;
                    }                   
                }
            }
            BUG("%1%: Unexpected extern method", extmeth->method->name.name);            
        } 
        else if (minst->is<P4::ExternFunction>()) {
            auto extfn = minst->to<P4::ExternFunction>();
            if (extfn->method->name.name == IR::ParserState::verify ||
                    extfn->method->name == v1model.clone.name ||
                    extfn->method->name == v1model.digest_receiver.name) {           
                BUG_CHECK(mce->arguments->size() == 2, "%1%: Expected 2 arguments", mce);
                return createExternFunction(2,mce,extfn->method->name,minst);
            }

            if (extfn->method->name == v1model.clone.clone3.name ||
                    extfn->method->name == v1model.random.name) {
                BUG_CHECK(mce->arguments->size() == 3, "%1%: Expected 3 arguments", mce);
                return createExternFunction(3,mce,extfn->method->name,minst);        
            }
            if (extfn->method->name == v1model.hash.name) {
                BUG_CHECK(mce->arguments->size() == 5, "%1%: Expected 5 arguments", mce);                
                static std::set<cstring> supportedHashAlgorithms = {
                    v1model.algorithm.crc32.name, v1model.algorithm.crc32_custom.name,
                    v1model.algorithm.crc16.name, v1model.algorithm.crc16_custom.name,
                    v1model.algorithm.random.name, v1model.algorithm.identity.name,
                    v1model.algorithm.csum16.name, v1model.algorithm.xor16.name
                };

                auto ei = P4::EnumInstance::resolve(mce->arguments->at(1), typemap);
                CHECK_NULL(ei);
                if (supportedHashAlgorithms.find(ei->name) == supportedHashAlgorithms.end()) {
                    ::error("%1%: unexpected algorithm", ei->name);
                    return nullptr;
                }
                return createExternFunction(5,mce,extfn->method->name,minst);          
            }
            if (extfn->method->name == v1model.resubmit.name ||
                extfn->method->name == v1model.recirculate.name ||
                extfn->method->name == v1model.truncate.name) {
                BUG_CHECK(mce->arguments->size() == 1, "%1%: Expected 1 argument", mce);                
                return createExternFunction(1,mce,extfn->method->name,minst);
            }
            if (extfn->method->name == v1model.drop.name) {
                BUG_CHECK(mce->arguments->size() == 0, "%1%: Expected 0 arguments", mce);                
                return createExternFunction(0,mce,extfn->method->name,minst);
            }

            BUG("%1%: Unexpected extern function", extfn->method->name.name.c_str());     
            return nullptr;
        } 
        else if (minst->is<P4::BuiltInMethod>()) {
            auto bi = minst->to<P4::BuiltInMethod>();
            if (bi->name == IR::Type_Header::isValid) {
                auto val = processExpression(bi->appliedTo,nullptr,nullptr,true);
                std::vector<Type*> args;
                args.push_back(val->getType());
                std::vector<Value*> params;
                params.push_back(val);
                FunctionType *FT = FunctionType::get(backend->Builder.getInt1Ty(), args, false);                
                Function *decl = Function::Create(FT, Function::ExternalLinkage, bi->name.name.c_str(), backend->TheModule.get());
                return backend->Builder.CreateCall(decl,params);
            } 
            else if (bi->name == IR::Type_Header::setValid || bi->name == IR::Type_Header::setInvalid) {
                auto val = processExpression(bi->appliedTo,nullptr,nullptr,true);
                std::vector<Type*> args;
                args.push_back(val->getType());
                std::vector<Value*> params;
                params.push_back(val);
                FunctionType *FT = FunctionType::get(backend->Builder.getVoidTy(), args, false);                
                Function *decl = Function::Create(FT, Function::ExternalLinkage, bi->name.name.c_str(), backend->TheModule.get());
                return backend->Builder.CreateCall(decl,params);
            } 
            else if (bi->name == IR::Type_Stack::push_front || bi->name == IR::Type_Stack::pop_front) {
                std::vector<Type*> args;
                args.push_back(backend->Builder.getInt32Ty());
                FunctionType *FT = FunctionType::get(backend->Builder.getVoidTy(), args, false);
                auto decl = backend->TheModule->getOrInsertFunction(bi->name.name.c_str(), FT);

                BUG_CHECK(mce->arguments->size() == 1, "Expected 1 argument for %1%", mce);
                
                llvmValue = processExpression(mce->arguments->at(0), nullptr, nullptr, true);
                assert(llvmValue != nullptr && "processExpression should not return null");
                auto tmp = llvmValue; llvmValue = nullptr;       
                return backend->Builder.CreateCall(decl, tmp);
            } 
            else {
                BUG("%1%: Unexpected built-in method", bi->name);
            }
            return nullptr;
        }
        else if(auto tbl = minst->to<P4::ApplyMethod>()) {
            if(tbl->isTableApply())
                convertTable(tbl->object->to<IR::P4Table>());            
        }
    }
    return nullptr;
}

bool ToIR::preorder(const IR::Declaration_Variable* t) {
    auto type = backend->getCorrespondingType(t->type);
    Value *alloca = backend->Builder.CreateAlloca(type);
    backend->st.insert("alloca_"+t->getName(),alloca);       
    return false;
}

bool ToIR::preorder(const IR::AssignmentStatement* t) {
    Value* leftValue = processExpression(t->left, nullptr, nullptr, true);
    assert(leftValue != nullptr && "left expression in assignment can't be null");
    Type* llvmType = leftValue->getType();          
    Value* right = processExpression(t->right);
    if(right != nullptr)    {
        auto eleTy = ((PointerType*)llvmType)->getElementType();
        if(eleTy->isVectorTy())    {
            right = backend->Builder.CreateBitCast(right, ((PointerType*)llvmType)->getElementType());
        }
        else if(eleTy->isIntegerTy() && (eleTy->getIntegerBitWidth() > right->getType()->getIntegerBitWidth())) 
            right = backend->Builder.CreateZExt(right, ((PointerType*)llvmType)->getElementType());
        backend->Builder.CreateStore(right,leftValue);           
    }
    else {
        BUG("Right part of assignment not found");
    }
    llvmValue = nullptr;    
    return false;
}

 bool ToIR::preorder(const IR::IfStatement* t) {
    BasicBlock* bbIf = BasicBlock::Create(backend->TheContext, "if.then", backend->function);
    BasicBlock* bbElse = BasicBlock::Create(backend->TheContext, "if.else", backend->function);
    Value* cond = processExpression(t->condition, bbIf, bbElse);
    BasicBlock* bbEnd = BasicBlock::Create(backend->TheContext, "if.end", backend->function);
    
    //To handle cases like if(a){//dosomething;}
    //Here 'a' is a PathExpression which would return a LoadInst on processing
    if(isa<LoadInst>(cond)) {
        cond = backend->Builder.CreateICmpEQ(cond, ConstantInt::get(cond->getType(),1));
    }

    backend->Builder.CreateCondBr(cond, bbIf, bbElse);
    backend->Builder.SetInsertPoint(bbIf);
    visit(t->ifTrue);
    backend->Builder.CreateBr(bbEnd);

    backend->Builder.SetInsertPoint(bbElse);
    visit(t->ifFalse);
    backend->Builder.CreateBr(bbEnd);
    
    backend->Builder.SetInsertPoint(bbEnd);
    return false;
}

bool ToIR::preorder(const IR::MethodCallStatement* stat) {
    auto mce = stat->to<IR::MethodCallStatement>()->methodCall;
    llvmValue = processExpression(mce);
    llvmValue = nullptr;
    return false;
}

bool ToIR::preorder(const IR::BlockStatement* b) {
    for(auto c : b->components)      
        visit(c);
    return false;
}

Value* ToIR::createTableFunction(int no, const IR::ConstructorCallExpression* mce, cstring name)   {
    std::vector<Type*> args;
    std::vector<Value*> param;
    
    for(int i=0; i<no; i++){
        if(auto inst = mce->arguments->at(i)->to<IR::ListExpression>())   {
            for(auto c : inst->components) {
                auto llvmValue = processExpression(c);
                assert(llvmValue != nullptr && "processExpression should not return null");
                args.push_back(llvmValue->getType());
                param.push_back(llvmValue);
                llvmValue = nullptr;
            }
        }
        else {
            auto llvmValue = processExpression(mce->arguments->at(i));
            assert(llvmValue != nullptr && "processExpression should not return null");
            args.push_back(llvmValue->getType());
            param.push_back(llvmValue);
            llvmValue = nullptr;
        } 
    }
    FunctionType *FT = FunctionType::get(backend->Builder.getInt8PtrTy(), args, false);
    auto decl = backend->TheModule->getOrInsertFunction(name.c_str(), FT);
    return backend->Builder.CreateCall(decl, param);
}

void ToIR::convertTableEntries(const IR::P4Table *table, Instruction* apply) {
    auto entriesList = table->getEntries();
    if (entriesList == nullptr) return;

    int entryPriority = 1;  // default priority is defined by index position
    SmallVector<Metadata*, 8> keyMDV;
    SmallVector<Metadata*, 8> actionMDV;
    SmallVector<Metadata*, 8> priorityMDV;    
    
    for (auto e : entriesList->entries) {
        auto keyset = e->getKeys();
        int keyIndex = 0;

        for (auto k : keyset->components) {
            auto tableKey = table->getKey()->keyElements.at(keyIndex);
            auto keyWidth = tableKey->expression->type->width_bits();
            auto k8 = ROUNDUP(keyWidth, 8);
            auto matchType = getKeyMatchType(tableKey);
            if (matchType == backend->getCoreLibrary().exactMatch.name) {
                if (k->is<IR::Constant>()){
                    keyMDV.push_back(MDString::get(backend->TheContext, stringRepr(k->to<IR::Constant>()->value, k8).c_str()));                
                }
                else if (k->is<IR::BoolLiteral>()) {
                    // booleans are converted to ints
                    keyMDV.push_back(MDString::get(backend->TheContext, stringRepr(k->to<IR::BoolLiteral>()->value ? 1 : 0, k8).c_str()));                                    
                }
                else
                    ::error("%1% unsupported exact key expression", k);
            } else if (matchType == backend->getCoreLibrary().ternaryMatch.name) {
                SmallVector<Metadata*, 8> tupleMDV;                
                if (k->is<IR::Mask>()) {
                    auto km = k->to<IR::Mask>();
                    tupleMDV.push_back(MDString::get(backend->TheContext, stringRepr(km->left->to<IR::Constant>()->value, k8).c_str()));
                    tupleMDV.push_back(MDString::get(backend->TheContext,stringRepr(km->right->to<IR::Constant>()->value, k8).c_str()));
                } else if (k->is<IR::Constant>()) {
                    tupleMDV.push_back(MDString::get(backend->TheContext,stringRepr(k->to<IR::Constant>()->value, k8).c_str()));
                    tupleMDV.push_back(MDString::get(backend->TheContext,stringRepr(Util::mask(keyWidth), k8).c_str()));                    
                } else if (k->is<IR::DefaultExpression>()) {
                    tupleMDV.push_back(MDString::get(backend->TheContext,stringRepr(0, k8).c_str()));
                    tupleMDV.push_back(MDString::get(backend->TheContext,stringRepr(0, k8).c_str()));                    
                } else {
                    ::error("%1% unsupported ternary key expression", k);
                }
                keyMDV.push_back(MDTuple::get(backend->TheContext, tupleMDV));
            } else if (matchType == backend->getCoreLibrary().lpmMatch.name) {
                SmallVector<Metadata*, 8> tupleMDV;                                
                if (k->is<IR::Mask>()) {
                    auto km = k->to<IR::Mask>();
                    tupleMDV.push_back(MDString::get(backend->TheContext,stringRepr(km->left->to<IR::Constant>()->value, k8).c_str()));
                    
                    auto trailing_zeros = [](unsigned long n) { return n ? __builtin_ctzl(n) : 0; };
                    auto count_ones = [](unsigned long n) { return n ? __builtin_popcountl(n) : 0;};
                    unsigned long mask = km->right->to<IR::Constant>()->value.get_ui();
                    auto len = trailing_zeros(mask);
                    if (len + count_ones(mask) != keyWidth)  // any remaining 0s in the prefix?
                        ::error("%1% invalid mask for LPM key", k);
                    else {
                        tupleMDV.push_back(MDString::get(backend->TheContext,std::to_string(keyWidth - len)));
                    }
                } else if (k->is<IR::Constant>()) {
                    tupleMDV.push_back(MDString::get(backend->TheContext,stringRepr(k->to<IR::Constant>()->value, k8).c_str()));                    
                    tupleMDV.push_back(MDString::get(backend->TheContext,std::to_string(keyWidth)));                    
                } else if (k->is<IR::DefaultExpression>()) {
                    tupleMDV.push_back(MDString::get(backend->TheContext,stringRepr(0, k8).c_str()));                                        
                    tupleMDV.push_back(MDString::get(backend->TheContext,0));                                        
                } else {
                    ::error("%1% unsupported LPM key expression", k);
                }
                keyMDV.push_back(MDTuple::get(backend->TheContext, tupleMDV));                
            } else if (matchType == "range") {
                SmallVector<Metadata*, 8> tupleMDV;                                                
                if (k->is<IR::Range>()) {
                    auto kr = k->to<IR::Range>();
                    tupleMDV.push_back(MDString::get(backend->TheContext,stringRepr(kr->left->to<IR::Constant>()->value, k8).c_str()));                                        
                    tupleMDV.push_back(MDString::get(backend->TheContext,stringRepr(kr->right->to<IR::Constant>()->value, k8).c_str()));                                        
                } else if (k->is<IR::DefaultExpression>()) {
                    tupleMDV.push_back(MDString::get(backend->TheContext,stringRepr(0, k8).c_str()));                                                            
                    tupleMDV.push_back(MDString::get(backend->TheContext,stringRepr((1 << keyWidth)-1, k8).c_str()));                                                            
                } else {
                    ::error("%1% invalid range key expression", k);
                }
                keyMDV.push_back(MDTuple::get(backend->TheContext, tupleMDV)); 
            } else {
                ::error("unkown key match type '%1%' for key %2%", matchType, k);
            }
            keyIndex++;
        }
                                      

        auto actionRef = e->getAction();
        if (!actionRef->is<IR::MethodCallExpression>())
            ::error("%1%: invalid action in entries list", actionRef);
        auto actionCall = actionRef->to<IR::MethodCallExpression>();
       
        actionMDV.push_back(MDString::get(backend->TheContext, actionCall->toString()));
        SmallVector<Metadata*, 8> tupleMDV;                                        
        for (auto arg : *actionCall->arguments) {
            tupleMDV.push_back(MDString::get(backend->TheContext,stringRepr(arg->to<IR::Constant>()->value, 0)));                                
        }

        auto priorityAnnotation = e->getAnnotation("priority");
        if (priorityAnnotation != nullptr) {
            if (priorityAnnotation->expr.size() > 1)
                ::error("invalid priority value %1%", priorityAnnotation->expr);
            auto priValue = priorityAnnotation->expr.front();
            if (!priValue->is<IR::Constant>())
                ::error("invalid priority value %1%. must be constant", priorityAnnotation->expr);
            priorityMDV.push_back(MDString::get(backend->TheContext, std::to_string(priValue->to<IR::Constant>()->value.get_si())));            
        } else {
            priorityMDV.push_back(MDString::get(backend->TheContext, std::to_string(entryPriority)));
        }
        entryPriority += 1;
    }
    MDNode* keysMD = MDNode::get(backend->TheContext, keyMDV);                        
    apply->setMetadata("key",keysMD); 

    MDNode* actionMD = MDNode::get(backend->TheContext, actionMDV);                        
    apply->setMetadata("action",actionMD);

    MDNode* priorityMD = MDNode::get(backend->TheContext, priorityMDV);                        
    apply->setMetadata("priority",priorityMD);
}


cstring ToIR::getKeyMatchType(const IR::KeyElement *ke) {
    auto path = ke->matchType->path;
    auto mt = refmap->getDeclaration(path, true)->to<IR::Declaration_ID>();
    BUG_CHECK(mt != nullptr, "%1%: could not find declaration", ke->matchType);

    if (mt->name.name == backend->getCoreLibrary().exactMatch.name ||
        mt->name.name == backend->getCoreLibrary().ternaryMatch.name ||
        mt->name.name == backend->getCoreLibrary().lpmMatch.name ||
        backend->getModel().find_match_kind(mt->name.name)) {
        return mt->name.name;
    }

    ::error("%1%: match type not supported on this target", mt);
    return "invalid";
}

Value* ToIR::handleTableImplementation(const IR::Property* implementation) {
    if (implementation == nullptr) {
        SmallVector<Type*,1> args;
        FunctionType *FT = FunctionType::get(backend->Builder.getInt8PtrTy(), args, false);
        auto decl = backend->TheModule->getOrInsertFunction("simplImpl", FT);
        return backend->Builder.CreateCall(decl);
    }

    if (!implementation->value->is<IR::ExpressionValue>()) {
        ::error("%1%: expected expression for property", implementation);
        return nullptr;
    }
    auto propv = implementation->value->to<IR::ExpressionValue>();

    if (propv->expression->is<IR::ConstructorCallExpression>()) {
        auto cc = P4::ConstructorCall::resolve(
            propv->expression->to<IR::ConstructorCallExpression>(),
            refmap, typemap);
        if (!cc->is<P4::ExternConstructorCall>()) {
            ::error("%1%: expected extern object for property", implementation);
            return nullptr;
        }
        auto ecc = cc->to<P4::ExternConstructorCall>();
        auto implementationType = ecc->type;
        auto arguments = ecc->cce->arguments;
        auto add_size = [&arguments](size_t arg_index) {
            auto size_expr = arguments->at(arg_index);
            if (!size_expr->is<IR::Constant>()) {
                ::error("%1% must be a constant", size_expr);
            }
        };
        if (implementationType->name == LLBMV2::TableImplementation::actionSelectorName) {
            BUG_CHECK(arguments->size() == 3, "%1%: expected 3 arguments", arguments);
            add_size(1);
            auto hash = arguments->at(0);
            auto ei = P4::EnumInstance::resolve(hash, typemap);
            if (ei == nullptr) {
                ::error("%1%: must be a constant on this target", hash);
            } else {
                return createTableFunction(3, ecc->cce, "actionSelector");
            }
        } else if (implementationType->name == LLBMV2::TableImplementation::actionProfileName) {
            return createTableFunction(1, ecc->cce, "actionProfile");            
        } else {
            ::error("%1%: unexpected value for property", propv);
        }
    } else if (propv->expression->is<IR::PathExpression>()) {
        auto pathe = propv->expression->to<IR::PathExpression>();
        auto decl = refmap->getDeclaration(pathe->path, true);
        if (!decl->is<IR::Declaration_Instance>()) {
            ::error("%1%: expected a reference to an instance", pathe);
            return nullptr;
        }
        auto dcltype = typemap->getType(pathe, true);
        if (!dcltype->is<IR::Type_Extern>()) {
            ::error("%1%: unexpected type for implementation", dcltype);
            return nullptr;
        }
        auto type_extern_name = dcltype->to<IR::Type_Extern>()->name;
        if ((type_extern_name != LLBMV2::TableImplementation::actionProfileName) ||
              (type_extern_name != LLBMV2::TableImplementation::actionSelectorName)) {
            ::error("%1%: unexpected type for implementation", dcltype);
            return nullptr;
        }
        auto eb = backend->getToplevelBlock()->getValue(decl->getNode());
        if (eb) {
            BUG_CHECK(eb->is<IR::ExternBlock>(), "Not an extern block?");
            // backend->getSimpleSwitch()->convertExternInstances(decl->to<IR::Declaration>(),
            //             eb->to<IR::ExternBlock>(), action_profiles, selector_check);
            assert(false && "Not yet handled!");
        }
    } else {
        ::error("%1%: unexpected value for property", propv);
        return nullptr;
    }
    return nullptr;
}

void ToIR::convertTable(const IR::P4Table* table) {
    LOG3("Processing " << dbp(table));
    cstring name = table->controlPlaneName();
    cstring table_match_type = backend->getCoreLibrary().exactMatch.name;
    auto key = table->getKey();
    std::vector<Type*> arg;
    std::vector<Value*> param;

    if (key != nullptr) {
        for (auto ke : key->keyElements) {
            auto expr = ke->expression;
            auto ket = typemap->getType(expr, true);
            if (!ket->is<IR::Type_Bits>() && !ket->is<IR::Type_Boolean>())
                ::error("%1%: Unsupported key type %2%", expr, ket);

            auto match_type = getKeyMatchType(ke);

            // if (match_type == LLBMV2::MatchImplementation::selectorMatchTypeName)
            //     continue;
            // Decreasing order of precedence (bmv2 specification):
            // 0) more than one LPM field is an error
            // 1) if there is at least one RANGE field, then the table is RANGE
            // 2) if there is at least one TERNARY field, then the table is TERNARY
            // 3) if there is a LPM field, then the table is LPM
            // 4) otherwise the table is EXACT
            if (match_type != table_match_type) {
                if (match_type == LLBMV2::MatchImplementation::rangeMatchTypeName)
                    table_match_type = LLBMV2::MatchImplementation::rangeMatchTypeName;
                if (match_type == backend->getCoreLibrary().ternaryMatch.name &&
                    table_match_type != LLBMV2::MatchImplementation::rangeMatchTypeName)
                    table_match_type = backend->getCoreLibrary().ternaryMatch.name;
                if (match_type == backend->getCoreLibrary().lpmMatch.name &&
                    table_match_type == backend->getCoreLibrary().exactMatch.name)
                    table_match_type = backend->getCoreLibrary().lpmMatch.name;
                else
                    table_match_type = match_type;
            } else if (match_type == backend->getCoreLibrary().lpmMatch.name) {
                ::error("%1%, Multiple LPM keys in table", table);
            }



            mpz_class mask;
            if (auto mexp = expr->to<IR::BAnd>()) {
                if (mexp->right->is<IR::Constant>()) {
                    mask = mexp->right->to<IR::Constant>()->value;
                    expr = mexp->left;
                } else if (mexp->left->is<IR::Constant>()) {
                    mask = mexp->left->to<IR::Constant>()->value;
                    expr = mexp->right;
                } else {
                    ::error("%1%: key mask must be a constant", expr); }
            } else if (auto slice = expr->to<IR::Slice>()) {
                expr = slice->e0;
                int h = slice->getH();
                int l = slice->getL();
                mask = Util::maskFromSlice(h, l);
            }

            Value *FBloc;
            auto it = backend->str.find(table_match_type.c_str());
            if(it != backend->str.end())   {
                FBloc = backend->str[table_match_type.c_str()];
            }
            else {
                Constant *fname = ConstantDataArray::getString(backend->TheContext,table_match_type.c_str(), true);
                FBloc = new GlobalVariable(*backend->function->getParent(),fname->getType(),true,GlobalValue::PrivateLinkage,fname);
                backend->str.insert(std::pair<std::string,Value*>(table_match_type.c_str(), FBloc));
            }
            param.push_back(FBloc);
            arg.push_back(FBloc->getType());
            auto val = processExpression(expr);
            auto type = val->getType();
            param.push_back(val);
            arg.push_back(type);
        }
    }
  
    std::vector<Value*> action_fnptr; 
    auto al = table->getActionList();
    
    for (auto a : al->actionList) {
        if (a->expression->is<IR::MethodCallExpression>()) {
            auto mce = a->expression->to<IR::MethodCallExpression>();
            if (mce->arguments->size() > 0)
                ::error("%1%: Actions in action list with arguments not supported", a);
        }
        auto decl = refmap->getDeclaration(a->getPath(), true);
        BUG_CHECK(decl->is<IR::P4Action>(), "%1%: should be an action name", a);
        auto action = decl->to<IR::P4Action>();
        auto name = action->controlPlaneName();

        Function* fn = backend->action_function[name];
        arg.push_back(fn->getType());
        param.push_back(fn);
    }

      Value* sizeVal;

    auto impl = table->properties->getProperty(LLBMV2::TableAttributes::implementationName);
    Value* val = handleTableImplementation(impl);
    assert(val != nullptr && "Table Implementation should be converted");
    param.push_back(val);
    arg.push_back(val->getType());
    unsigned size = 0;
    auto sz = table->properties->getProperty(LLBMV2::TableAttributes::sizeName);
    if (sz != nullptr) {
        if (sz->value->is<IR::ExpressionValue>()) {
            auto expr = sz->value->to<IR::ExpressionValue>()->expression;
            if (!expr->is<IR::Constant>()) {
                ::error("%1% must be a constant", sz);
                size = 0;
            } else {
                size = expr->to<IR::Constant>()->asInt();
            }
        } else {
            ::error("%1%: expected a number", sz);
        }
    }
    if (size == 0)
        size = LLBMV2::TableAttributes::defaultTableSize;

    sizeVal = ConstantInt::get(Type::getInt32Ty(backend->TheContext), size);
    arg.push_back(sizeVal->getType());
    param.push_back(sizeVal);

    auto defact = table->properties->getProperty(IR::TableProperties::defaultActionPropertyName);
    if (defact != nullptr) {
        if (!defact->value->is<IR::ExpressionValue>()) {
            ::error("%1%: expected an action", defact);
        }
        auto expr = defact->value->to<IR::ExpressionValue>()->expression;
        const IR::P4Action* action = nullptr;

        if (expr->is<IR::PathExpression>()) {
            auto path = expr->to<IR::PathExpression>()->path;
            auto decl = refmap->getDeclaration(path, true);
            BUG_CHECK(decl->is<IR::P4Action>(), "%1%: should be an action name", expr);
            action = decl->to<IR::P4Action>();
        } else if (expr->is<IR::MethodCallExpression>()) {
            auto mce = expr->to<IR::MethodCallExpression>();
            auto mi = P4::MethodInstance::resolve(mce,
                    refmap, typemap);
            BUG_CHECK(mi->is<P4::ActionCall>(), "%1%: expected an action", expr);
            action = mi->to<P4::ActionCall>()->action;
        } else {
            ::error("%1%: unexpected expression", expr);
        }

        Function* fn = backend->action_function[action->controlPlaneName()];
        arg.push_back(fn->getType());
        param.push_back(fn);
    }
    FunctionType *FT = FunctionType::get(backend->Builder.getVoidTy(), arg, false);
    Function *decl = Function::Create(FT, Function::ExternalLinkage,  "apply_"+name, backend->TheModule.get());
    auto call = backend->Builder.CreateCall(decl, param);
    convertTableEntries(table, call);
}