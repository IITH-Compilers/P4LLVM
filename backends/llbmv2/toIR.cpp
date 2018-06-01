#include "toIR.h"

void ToIR::createExternFunction(int no, const IR::MethodCallExpression* mce, cstring name)   {
    std::vector<Type*> args;
    std::vector<Value*> param;
    
    for(int i=0; i<no; i++){
        if(auto inst = mce->arguments->at(i)->to<IR::ListExpression>())   {
            std::cout << "caught list exp\n";
            for(auto c : inst->components) {
                std::cout << "processing - " << c<<"\n";
                llvmValue = processExpression(c);
                args.push_back(llvmValue->getType());
                assert(llvmValue != nullptr && "processExpression should not return null");
                param.push_back(llvmValue);
                llvmValue = nullptr;
            }
        }
        else {
            llvmValue = processExpression(mce->arguments->at(i));
            args.push_back(llvmValue->getType());
            assert(llvmValue != nullptr && "processExpression should not return null");
            param.push_back(llvmValue);
            llvmValue = nullptr;
        } 
    }

    FunctionType *FT = FunctionType::get(backend->Builder.getVoidTy(), args, false);
    Function *decl = Function::Create(FT, Function::ExternalLinkage,  name.c_str(), backend->TheModule.get());
    backend->Builder.CreateCall(decl, param);
}

Value* ToIR::processExpression(const IR::Expression* e, BasicBlock* bbIf/*=nullptr*/, BasicBlock* bbElse/*=nullptr*/, bool required_alloca /*=false*/) {

    assert(e != nullptr);
    std::cout <<"processing -- " << *e << "\n";
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

        if(auto c = e->to<IR::Cast>())  {
            std::cout << "in cast\n"<<"oue->expr -- " <<*oue->expr<<"\ncast type--"<<*c<<"\n";
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
            std::cout << "inside member handling of processexpression\n";
            const IR::Member* expression = e->to<IR::Member>();        
            auto type = backend->getTypeMap()->getType(expression, true);
            if (type->is<IR::Type_Error>() && expression->expr->is<IR::TypeNameExpression>()) {
                // this deals with constants that have type 'error'
                MYDEBUG(std::cout<<"TYPE_ERROR\n";)            
                auto decl = type->to<IR::Type_Error>()->getDeclByName(expression->member.name);
                ErrorCodesMap errCodes = backend->getErrorCodesMap();
                auto errorValue = errCodes.at(decl);
                return ConstantInt::get(backend->Builder.getInt32Ty(),errorValue);        
            }
            if (type->is<IR::Type_Enum>() && expression->expr->is<IR::TypeNameExpression>()) {
                std::cout << "TYPE_ENUM\n";
                auto decl = type->to<IR::Type_Enum>()->getDeclByName(expression->member.name)->getName();
                std::cout << decl <<"\n" << type->to<IR::Type_Enum>()->name; 
                auto it = backend->enums.find(type->to<IR::Type_Enum>()->name);  
                if(it != backend->enums.end()) {
                    auto itr = std::find(it->second.begin(), it->second.end(), decl);
                    // if(itr != it.second.end())
                    auto enumValue = std::distance(it->second.begin(), itr)+1;
                    // auto enumValue = it->second.at(decl);
                    return ConstantInt::get(backend->Builder.getInt32Ty(),enumValue);                     
                }
                else {
                    std::vector<cstring> enumContainer; 
                    for (auto e : *(type->to<IR::Type_Enum>())->getDeclarations()) {
                        enumContainer.push_back(e->getName().name);
                        std::cout << "\n-------\n" << e->getName().name << "\n-------\n";
                    }   
                    auto itr = std::find(enumContainer.begin(), enumContainer.end(), decl);                    
                    // auto enumValue = enumContainer.at(decl);                    
                    auto enumValue = std::distance(enumContainer.begin(), itr)+1;                    
                    backend->enums.insert(make_pair(type->to<IR::Type_Enum>()->name,enumContainer));
                    return ConstantInt::get(backend->Builder.getInt32Ty(),enumValue);                                         
                }
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
            std::cout << "*****************************************************************************\n";
            if(ex->getType()->getPointerElementType()->isVectorTy()) {
                std::cout << "inside arrayty\n";
                // ex->dump();
                // ex->getType()->getPointerElementType()->dump();
                auto width = dyn_cast<SequentialType>(ex->getType()->getPointerElementType())->getNumElements();
                std::cout << "width =-- "<<width<<"!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!111111111\n";
                auto x = Type::getIntNPtrTy(backend->TheContext, width);
                auto tmp = backend->Builder.CreateBitCast(ex, x);
                // tmp->dump();
                ex = tmp;
            }
            // MYDEBUG(ex->dump();)
            if (required_alloca) 
                return ex;
            else {
                // backend->Builder.CreateLoad(ex)->dump();
                return backend->Builder.CreateLoad(ex);
            }
        }
        else
            BUG("unhandled op_unary");
    }
    if(e->is<IR::BoolLiteral>())    {
        std::cout << "caught as boolliteral\n";
        
        const IR::BoolLiteral* c = e->to<IR::BoolLiteral>();            
        return ConstantInt::get(backend->getType(typemap->getType(c)),c->value);            
    }

    if(e->is<IR::Constant>()) {  
        std::cout << "caught as constant -- "<<*e<<"\n";
        
        const IR::Constant* c = e->to<IR::Constant>();
        auto ty = typemap->getType(c);
        if(!ty)
            ty = c->type;
        return ConstantInt::get(backend->getType(ty),(c->value).get_si());
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

        // std::cout<<"left-->"<<*obe->left<<"\n";left->dump();std::cout<<"right-->"<<*obe->right<<"\n";right->dump();
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
            
            std::cout <<"left -- ";
            // left->dump();
            // left->getType()->dump();
            std::cout <<"right -- ";
            // right->dump();
            // right->getType()->dump();
            

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
        std::cout << "caught in op ternary\n";
        const IR::Operation_Ternary* ote = e->to<IR::Operation_Ternary>();
        Value* e0 = processExpression(ote->e0);
        Value* e1 = processExpression(ote->e1);
        Value* e2 = processExpression(ote->e2);

        if(e->is<IR::Mux>())    {
            return backend->Builder.CreateSelect(e0, e1, e2);
        }

    }

    if(e->is<IR::MethodCallExpression>())    {
        MYDEBUG(std::cout<<"caught mcs\n";)
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
                FunctionType *FT = FunctionType::get(backend->getType(typearg), args, false);
                Function *function = Function::Create(FT, Function::ExternalLinkage,  
                                            em->method->name.name.c_str(), backend->TheModule.get());
                return backend->Builder.CreateCall(function);    
            }
        }
    }
    
    ::error("Returning nullptr in processExpression");        
    return nullptr;
}

bool ToIR::preorder(const IR::Declaration_Variable* t) {
    auto type = backend->getCorrespondingType(typemap->getType(t));
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
        std::cout << "in assignment stmt -- right\nleft val -- ";
        // leftValue->dump();
        std::cout<<"right val--\n";
        // right->dump();
        if(((PointerType*)llvmType)->getElementType()->isVectorTy())    {
            std::cout << "im vector type\n";
            // ((PointerType*)llvmType)->getElementType()->dump();
            right = backend->Builder.CreateBitCast(right, ((PointerType*)llvmType)->getElementType());
        }
        else if(((PointerType*)llvmType)->getElementType ()->getIntegerBitWidth() > right->getType()->getIntegerBitWidth())
            right = backend->Builder.CreateZExt(right, llvmType);
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
    // if(t->condition->is<IR::MethodCallStatement>())  {MYDEBUG(std::cout<<"its true";)}
    Value* cond = processExpression(t->condition, bbIf, bbElse);
    MYDEBUG(std::cout << "processed expression for if condition\n";)
    BasicBlock* bbEnd = BasicBlock::Create(backend->TheContext, "if.end", backend->function);
    
    //To handle cases like if(a){//dosomething;}
    //Here 'a' is a PathExpression which would return a LoadInst on processing
    if(isa<LoadInst>(cond)) {
        MYDEBUG(std::cout << "Its a load inst\n";)
        // cond->dump();
        // cond->getType()->dump();
        cond = backend->Builder.CreateICmpEQ(cond, ConstantInt::get(cond->getType(),1));
        MYDEBUG(std::cout << "created a icmp eq\n";)
    }

    backend->Builder.CreateCondBr(cond, bbIf, bbElse);
    MYDEBUG(std::cout<< "SetInsertPoint = Ifcondition\n";)
    backend->Builder.SetInsertPoint(bbIf);
    visit(t->ifTrue);
    backend->Builder.CreateBr(bbEnd);

    MYDEBUG(std::cout<< "SetInsertPoint = Else Condition\n";)
    backend->Builder.SetInsertPoint(bbElse);
    visit(t->ifFalse);
    backend->Builder.CreateBr(bbEnd);
    
    MYDEBUG(std::cout<< "SetInsertPoint = IfEnd\n";)
    backend->Builder.SetInsertPoint(bbEnd);
    return false;
}

bool ToIR::preorder(const IR::MethodCallStatement* stat) {
    auto mce = stat->to<IR::MethodCallStatement>()->methodCall;
    auto minst = P4::MethodInstance::resolve(mce, refmap, typemap);
    if (minst->is<P4::ExternMethod>()) {
        auto extmeth = minst->to<P4::ExternMethod>();
        if (extmeth->method->name.name == corelib.packetIn.extract.name) {
            int argCount = mce->arguments->size();
            if (argCount == 1 || argCount == 2) {
                auto arg = mce->arguments->at(0);
                auto argtype = typemap->getType(arg, true);
                if (!argtype->is<IR::Type_Header>()) {
                    ::error("%1%: extract only accepts arguments with header types, not %2%",
                            arg, argtype);
                    return false;
                }
                
                if (argCount == 1) {
                    std::cout << "calling processexp from mcs of convert parser stmt: ac=1\n";
                    createExternFunction(1,mce,extmeth->method->name.name);
                    return false;                      
                }
                // if (arg->is<IR::Member>()) {
                //     auto mem = arg->to<IR::Member>();
                //     auto baseType = typemap->getType(mem->expr, true);
                //     if (baseType->is<IR::Type_Stack>()) {
                //         if (mem->member == IR::Type_Stack::next) {
                //             type = "stack";
                //             j = conv->convert(mem->expr);
                //         } else {
                //             BUG("%1%: unsupported", mem);
                //         }
                //     }
                // }

                if (argCount == 2) {
                    createExternFunction(2,mce,extmeth->method->name.name);
                    return false;
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
            createExternFunction(2,mce,extfn->method->name);
            return false;
        }

        if (extfn->method->name == v1model.clone.clone3.name ||
                extfn->method->name == v1model.random.name) {
            BUG_CHECK(mce->arguments->size() == 3, "%1%: Expected 3 arguments", mce);
            createExternFunction(3,mce,extfn->method->name);
            return false;         
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
                return false;
            }
            createExternFunction(5,mce,extfn->method->name);
            return false;          
        }
        if (extfn->method->name == v1model.resubmit.name ||
               extfn->method->name == v1model.recirculate.name ||
               extfn->method->name == v1model.truncate.name) {
            BUG_CHECK(mce->arguments->size() == 1, "%1%: Expected 1 argument", mce);                
            createExternFunction(1,mce,extfn->method->name);
            return false; 
        }
        if (extfn->method->name == v1model.drop.name) {
            BUG_CHECK(mce->arguments->size() == 0, "%1%: Expected 0 arguments", mce);                
            createExternFunction(0,mce,extfn->method->name);
            return false; 
        }

        BUG("%1%: Unexpected extern function", extfn->method->name.name.c_str());     
        return false;
    } 
    else if (minst->is<P4::BuiltInMethod>()) {
        auto bi = minst->to<P4::BuiltInMethod>();
        if (bi->name == IR::Type_Header::setValid || bi->name == IR::Type_Header::setInvalid) {
            auto decl = backend->TheModule->getOrInsertFunction(bi->name.name.c_str(), backend->Builder.getVoidTy());
            backend->Builder.CreateCall(decl);
        } 
        else if (bi->name == IR::Type_Stack::push_front || bi->name == IR::Type_Stack::pop_front) {
            std::vector<Type*> args;
            args.push_back(backend->Builder.getInt32Ty());
            FunctionType *FT = FunctionType::get(backend->Builder.getVoidTy(), args, false);
            auto decl = backend->TheModule->getOrInsertFunction(bi->name.name.c_str(), FT);

            BUG_CHECK(mce->arguments->size() == 1, "Expected 1 argument for %1%", mce);
            
            std::cout << "calling processexp from builtin meth of convert parser stmt:1\n";                                    
            llvmValue = processExpression(mce->arguments->at(0), nullptr, nullptr, true);
            assert(llvmValue != nullptr && "processExpression should not return null");
            backend->Builder.CreateCall(decl, llvmValue);
            llvmValue = nullptr;
            
        } 
        else {
            BUG("%1%: Unexpected built-in method", bi->name);
        }
        return false;
    }
    std::cout << "----------------------unhandled mcs---------------------------------------\n";
    return false;
}

bool ToIR::preorder(const IR::BlockStatement* b) {
    for(auto c : b->components)      
        visit(c);
    return false;
}
