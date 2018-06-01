#include "toIR.h"

Value* ToIR::processExpression(const IR::Expression* e, BasicBlock* bbIf/*=nullptr*/, BasicBlock* bbElse/*=nullptr*/, bool required_alloca /*=false*/) {

    assert(e != nullptr);
    if(e->is<IR::Operation_Unary>())    {
        std::cout << "caught as operation_unary\n"<<*e<<"\n";
        const IR::Operation_Unary* oue = e->to<IR::Operation_Unary>();
        Value* exp = processExpression(oue->expr, bbIf, bbElse);
        if(e->is<IR::Cmpl>()) 
            return backend->Builder.CreateXor(exp,-1);

        if(e->is<IR::Neg>())    
            return backend->Builder.CreateSub(ConstantInt::get(exp->getType(),0), exp);

        if(e->is<IR::LNot>())
            return backend->Builder.CreateICmpEQ(exp, ConstantInt::get(exp->getType(),0));

        if (e->is<IR::Member>()) {
            std::cout << "inside member handling of processexpression\n";
            const IR::Member* expression = e->to<IR::Member>();        
            auto type = backend->getTypeMap()->getType(expression, true);
            if (type->is<IR::Type_Error>() && expression->expr->is<IR::TypeNameExpression>()) {
                // this deals with constants that have type 'error'
                MYDEBUG(std::cout<<"caught error\n";)            
                auto decl = type->to<IR::Type_Error>()->getDeclByName(expression->member.name);
                ErrorCodesMap errCodes = backend->getErrorCodesMap();
                auto errorValue = errCodes.at(decl);
                return ConstantInt::get(backend->Builder.getInt32Ty(),errorValue);        
            }
            Value *ex = processExpression(e->to<IR::Member>()->expr, nullptr, nullptr, true);
            // ex->dump();
            MYDEBUG(std::cout << e->to<IR::Member>()->member.name << std::endl;)
            int ext_i = backend->structIndexMap[((PointerType *)ex->getType())->getElementType()][std::string(e->to<IR::Member>()->member.name)];
            MYDEBUG(std::cout << "processed expression inside member, retrieved from backend->structIndexMap as - " << ext_i << std::endl;)
            std::vector<Value *> idx;
            idx.push_back(ConstantInt::get(backend->TheContext, llvm::APInt(32, 0, false)));
            idx.push_back(ConstantInt::get(backend->TheContext, llvm::APInt(32, ext_i, false)));
            ex = backend->Builder.CreateGEP(ex, idx);
            MYDEBUG(std::cout << "created GEP\n";)
            // MYDEBUG(ex->dump();)
            if (required_alloca) return ex;
            else return backend->Builder.CreateLoad(ex);
        }
        else
            BUG("unhandled op_unary");
    }
    if(e->is<IR::BoolLiteral>())    {
        std::cout << "caught as boolliteral\n";
        
        const IR::BoolLiteral* c = e->to<IR::BoolLiteral>();            
        return ConstantInt::get(backend->getCorrespondingType(typemap->getType(c)),c->value);            
    }

    if(e->is<IR::Constant>()) {  
        std::cout << "caught as constant\n";
        
        const IR::Constant* c = e->to<IR::Constant>();
        return ConstantInt::get(backend->getCorrespondingType(typemap->getType(c)),(c->value).get_si());
    }


    if(e->is<IR::PathExpression>()) {
        cstring name = refmap->getDeclaration(e->to<IR::PathExpression>()->path)->getName();    
        Value* v = backend->st.lookupGlobal("alloca_"+name);
        std::cout << "looking for -- alloca_"<< name << " in pathexpression of processexpression()\n";
        assert(v != nullptr);
        if(required_alloca)
            return v;
        else   {
            std::cout << "creating load..\n";
            auto a = backend->Builder.CreateLoad(v);
            // a->dump();
            return a;
        }
    }


    if(e->is<IR::Operation_Binary>())   {
        std::cout << "caught as operation_binary\n";
        
        const IR::Operation_Binary* obe = e->to<IR::Operation_Binary>();
        Value* left; Value* right;
        left = processExpression(obe->left, bbIf, bbElse);
        
        if(!(e->is<IR::LAnd>() || e->is<IR::LOr>()))    {
            right = processExpression(obe->right, bbIf, bbElse);
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
            MYDEBUG(std::cout<< "SetInsertPoint = land.rhs\n";)
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
            MYDEBUG(std::cout<< "SetInsertPoint = land.end\n";)
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
            MYDEBUG(std::cout<< "SetInsertPoint = land.rhs\n";)
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
            MYDEBUG(std::cout<< "SetInsertPoint = land.end\n";)
            backend->Builder.SetInsertPoint(bbTrue);
            PHINode* phi = backend->Builder.CreatePHI(icmp1->getType(), 2);
            phi->addIncoming(ConstantInt::getTrue(backend->TheContext), bbParent);
            phi->addIncoming(icmp2, bbFalse);
            return phi;
        }

        if(e->is<IR::Operation_Relation>()) {
            std::cout << "caught as Operation_Relation\n";
            
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

    /*if(e->is<IR::Operation_Ternary>())    {
        const IR::Operation_Ternary* ote = e->to<IR::Operation_Ternary>();
        Value* e0 = processExpression(ote->e0);
        Value* e1 = processExpression(ote->e1);
        Value* e2 = processExpression(ote->e2);

        //if(e->is<IR::Mux>())
                //Not required as statements are converted automatically to if-else form by other passes.

    }*/

    if(e->is<IR::MethodCallExpression>())    {
        MYDEBUG(std::cout<<"caught mcs\n";)
        // visit(e);
        const IR::MethodCallExpression* mce = e->to<IR::MethodCallExpression>();            
        auto instance = P4::MethodInstance::resolve(mce,refmap,typemap);

        if (instance->is<P4::ExternMethod>()) {
            auto em = instance->to<P4::ExternMethod>();
            if (em->originalExternType->name == corelib.packetIn.name &&
                em->method->name == corelib.packetIn.lookahead.name) {
                BUG_CHECK(mce->typeArguments->size() == 1,
                        "Expected 1 type parameter for %1%", em->method);
                auto targ = mce->typeArguments->at(0);
                auto typearg = backend->getTypeMap()->getTypeType(targ, true);
                int width = typearg->width_bits();
                BUG_CHECK(width > 0, "%1%: unknown width", targ);

                std::vector<Type*> args;
                FunctionType *FT = FunctionType::get(backend->getCorrespondingType(typearg), args, false);
                Function *function = Function::Create(FT, Function::ExternalLinkage,  
                                            em->method->name.name.c_str(), backend->TheModule.get());
                return backend->Builder.CreateCall(function);    
            }
        }
    }
    ::error("Returning nullptr in processExpression");        
    return nullptr;
}

