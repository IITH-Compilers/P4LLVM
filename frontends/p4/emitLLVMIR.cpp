#include "emitLLVMIR.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/DerivedTypes.h"

namespace P4	{




	///// --> To do very big bit size 
    unsigned EmitLLVMIR::getByteAlignment(unsigned width) {
    	if (width <= 8)
    	    return 8;
    	else if (width <= 16)
    	    return 16;
    	else if (width <= 32)
    	    return 32;
    	else if (width <= 64)
    	    return 64;
    	else
    	    // compiled as u8* 
    	    return 64;
    }

    bool EmitLLVMIR::preorder(const IR::Declaration_Variable* t)
    {
	    AllocaInst *alloca = Builder.CreateAlloca(getCorrespondingType(typeMap->getType(t)));
    	st.insert("alloca_"+t->getName(),alloca);		
    	std::cout<<"\nDeclaration_Variable\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;
    	return true;
    }

    bool EmitLLVMIR::preorder(const IR::Type_StructLike* t)
    {
    	std::cout<<"\nType_StructLike\t "<<*t << "\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
    	auto z = defined_type[t->externalName()];
    	if(z)
    	{
    		return(defined_type[t->externalName()]);
    	}

    	AllocaInst* alloca;
    	std::vector<Type*> members;
    	for(auto x: t->fields)
    	{
	    	members.push_back(getCorrespondingType(x->type)); // for int and all
    	}
    	StructType *structReg = llvm::StructType::create(TheContext, members, "struct."+t->name); // 
		defined_type[t->externalName()] = structReg;

    	alloca = Builder.CreateAlloca(structReg);
    	st.insert("alloca_"+t->getName(),alloca);
    	return true;
    }




    bool EmitLLVMIR::preorder(const IR::Type_Extern* t)
    {
        std::cout<<"\nType_Extern\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
           
        // typeParameters --> parameters
        // getDeclarations --> Declarations
        visit(t->typeParameters);
        preorder(&t->methods);
        return true;
    }

    bool EmitLLVMIR::preorder(const IR::ParameterList* t)
    {
        std::cout<<"\nParameterList\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        for (auto param : *t->getEnumerator())
        {
            std::cout << "flag2: " << param << std::endl;
            visit(param); // visits parameter
        }
        return true;
    }


    // TODO - Pass by Reference when direction is out/inout
    bool EmitLLVMIR::preorder(const IR::Parameter* t)
    {
        std::cout<<"\nParameterX\t "<<*t << " | " << "Type = " << t->type  << "||" << typeMap->getType(t) << "\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        // std::cout << "FLAG : " <<  << std::endl;
        visit(t->annotations);
        AllocaInst *alloca = Builder.CreateAlloca(getCorrespondingType(t->type));
        st.insert("alloca_"+t->getName(),alloca);
        // switch (t->direction)
        // {
        //     case IR::Direction::None:
        //         alloca = Builder.CreateAlloca(getCorrespondingType(t->type));
        //         st.insert("alloca_"+t->getName(),alloca);
        //         break;
        //     case IR::Direction::In:
        //         alloca = Builder.CreateAlloca(getCorrespondingType(t->type));
        //         st.insert("alloca_"+t->getName(),alloca);
        //         break;
        //     case IR::Direction::Out:
        //         alloca = Builder.CreateAlloca(getCorrespondingType(t->type));
        //         st.insert("alloca_"+t->getName(),alloca);
        //         break;
        //     case IR::Direction::InOut: // must be 
        //         alloca = Builder.CreateAlloca(getCorrespondingType(t->type));
        //         st.insert("alloca_"+t->getName(),alloca);
        //         break;
        //     default:
        //         MYDEBUG(std::cout << "Unexpected Case Found \n";);
        //         BUG("Unexpected case");
        // }
        return true;
    }


    // To Check
    bool EmitLLVMIR::preorder(const IR::TypeParameters* t) // type of method?
    {
        std::cout<<"\nTypeParameters\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";

        if (!t->empty())
        {
            for (auto a : t->parameters)
            {
                visit(a); // visits Type_Var
                // AllocaInst *alloca = Builder.CreateAlloca(getCorrespondingType(typeMap->getType(t)));
                // st.insert("alloca_"+t->getName(),alloca);


            }
        }
        return true;
    }


    bool EmitLLVMIR::preorder(const IR::Type_Var* t)
    {
        // t->getVarName()
        // t->getP4Type()
        std::cout<<"\nType_Var\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }


    // Completed
    bool EmitLLVMIR::preorder(const IR::P4Parser* t)
    {
        std::cout<<"\nTP4Parser\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";


        visit(t->type); // visits type_parser

        if (t->constructorParams->size() != 0) 
            visit(t->constructorParams); //visits Vector -> ParameterList

        visit(&t->parserLocals); // visits Vector -> Declaration 

        //Declare basis block for each state
        for (auto s : t->states)  
        {
            llvm::BasicBlock* bbInsert = llvm::BasicBlock::Create(TheContext, std::string(s->name.name), function);
            defined_state[s->name.name] = bbInsert;
            MYDEBUG(std::cout << s->name.name << std::endl;);
        }

        // visit all states
        visit(&t->states);
        // for (auto s : t->states)
        // {
        //     MYDEBUG(std::cout << "Visiting State = " <<  s << std::endl;);
        //     visit(s);
        // }

        Builder.SetInsertPoint(defined_state["accept"]); // on exit set entry of new inst in accept block
        return true;
    }


    // To check for bugs after 
    bool EmitLLVMIR::preorder(const IR::ParserState* parserState)
    {
        std::cout<<"\nParserState\t "<<*parserState<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        

        if(parserState->name == "start")
        {
            // if start state then create branch
            // from main function to start block
            Builder.CreateBr(defined_state[parserState->name.name]);
        }

        // set this block as insert point
        Builder.SetInsertPoint(defined_state[parserState->name.name]);
        if (parserState->name == "accept")
        {
            return true;

        }
        else if(parserState->name == "reject")
        {
            Builder.CreateRetVoid();
            return true;
        }
        
        preorder(&parserState->components); // indexvector -> statordecl


        // ----------------------  Check From Here  ----------------------

        // if  select expression is null
        
        if (parserState->selectExpression == nullptr) // Create branch to reject(there should be some transition or select)
        {
            MYDEBUG(std::cout<< parserState->selectExpression << std::endl;);
            Builder.CreateBr(defined_state["reject"]); // if reject then return 0;
        } 
        // visit SelectExpression
        else if (parserState->selectExpression->is<IR::SelectExpression>())
        {
            visit(parserState->selectExpression);
        } 
        //  Else transition expression
        else
        {
            // must be a PathExpression which is goto 'state' state name
            if (parserState->selectExpression->is<IR::PathExpression>())
            {
                const IR::PathExpression *x = dynamic_cast<const IR::PathExpression *>( parserState->selectExpression);
                Builder.CreateBr(defined_state[x->path->asString()]);
            }
            // bug 
            else
            {
                REPORTBUG(std::cout << __FILE__ << ":" << __LINE__ << ": Got Unknown Expression type" << std::endl;);
            }
        }
        return true;
    }
    



    bool EmitLLVMIR::preorder(const IR::Type_Parser* t)
    {
        // auto pl = t->applyParams;
        std::cout<<"\nType_Parser\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        visit(t->annotations);
        visit(t->typeParameters);    // visits TypeParameters
        visit(t->applyParams);       // visits ParameterList
        return true;
    }



    // bool EmitLLVMIR::preorder(const IR::P4Control* t)
    // {
    //     MYDEBUG(std::cout << t->type << std::endl;);
    //     llvm::BasicBlock* bbInsert = llvm::BasicBlock::Create(TheContext, "control_block", function);
    //     Builder.SetInsertPoint(bbInsert);

    //     return true; // why true and why false ? // Here true works 
    // }






    bool EmitLLVMIR::preorder(const IR::AssignmentStatement* t)
    {
        std::cout<<"\nAssignmentStatement\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        llvm::Type* llvmType = nullptr;

        if(t->left->is<IR::PathExpression>())   {
            cstring name = refMap->getDeclaration(t->left->to<IR::PathExpression>()->path)->getName();
            Value* v = st.lookupLocal("alloca_"+name);

            // assert(v != nullptr);
            if(v==nullptr)
            {
                return true;
            }

			llvmType = defined_type[typeMap->getType(t->left)->toString()];
			assert(llvmType != nullptr);

			Value* right = processExpression(t->right);
			
			if(right != nullptr)	{
				if(llvmType->getIntegerBitWidth() > right->getType()->getIntegerBitWidth())
					right = Builder.CreateZExt(right, llvmType);
				//store 
				Builder.CreateStore(right,v);			
			}
			
		}

		// else	{
		// 	//To-Do
		// }
        
        return true;
    }




    bool EmitLLVMIR::preorder(const IR::Type_Control* t)
    {
        std::cout<<"\nType_Control\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        visit(t->annotations);
        visit(t->typeParameters);
        visit(t->applyParams);                   
        return true;
    }
    bool EmitLLVMIR::preorder(const IR::P4Control* t)
    {
        std::cout<<"\nP4Control\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        visit(t->type); // visits Type_Control
        visit(t->constructorParams); // ParameterList
        visit(&t->controlLocals); //IndexedVector<Declaration>
        visit(t->body); // BlockStatement
        return true;
    }

	bool EmitLLVMIR::preorder(const IR::IfStatement* t)	{
		std::cout<<"\nIfStatement\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
		
		BasicBlock* bbIf = BasicBlock::Create(TheContext, "if.then", function);
		BasicBlock* bbElse = BasicBlock::Create(TheContext, "if.else", function);
		Value* cond = processExpression(t->condition, bbIf, bbElse);
		BasicBlock* bbEnd = BasicBlock::Create(TheContext, "if.end", function);
		
		//To handle cases like if(a){//dosomething;}
		//Here 'a' is a PathExpression which would return a LoadInst on processing
		if(isa<LoadInst>(cond))	
			cond = Builder.CreateICmpEQ(cond, ConstantInt::get(cond->getType(),1));

		Builder.CreateCondBr(cond, bbIf, bbElse);

		Builder.SetInsertPoint(bbIf);
		visit(t->ifTrue);
		Builder.CreateBr(bbEnd);

		Builder.SetInsertPoint(bbElse);
		visit(t->ifFalse);
		Builder.CreateBr(bbEnd);

		Builder.SetInsertPoint(bbEnd);
		return true;
	}

	Value* EmitLLVMIR::processExpression(const IR::Expression* e, BasicBlock* bbIf/*=nullptr*/, BasicBlock* bbElse/*=nullptr*/)	{
		assert(e != nullptr);

		if(e->is<IR::Operation_Unary>())	{
			const IR::Operation_Unary* oue = e->to<IR::Operation_Unary>();
			Value* exp = processExpression(oue->expr, bbIf, bbElse);
			if(e->is<IR::Cmpl>()) 
				return Builder.CreateXor(exp,-1);

			if(e->is<IR::Neg>())	
				return Builder.CreateSub(ConstantInt::get(exp->getType(),0), exp);

			if(e->is<IR::LNot>())
				return Builder.CreateICmpEQ(exp, ConstantInt::get(exp->getType(),0));
		}
		if(e->is<IR::BoolLiteral>())	{
			const IR::BoolLiteral* c = e->to<IR::BoolLiteral>();			
			return ConstantInt::get(getCorrespondingType(typeMap->getType(c)),c->value);			
		}

		if(e->is<IR::Constant>()) {  
			const IR::Constant* c = e->to<IR::Constant>();
			return ConstantInt::get(getCorrespondingType(typeMap->getType(c)),(c->value).get_si());
		}


		if(e->is<IR::PathExpression>())	{
			cstring name = refMap->getDeclaration(e->to<IR::PathExpression>()->path)->getName();	
			Value* v = st.lookupLocal("alloca_"+name);
			assert(v != nullptr);
			return Builder.CreateLoad(v);
		}

		if(e->is<IR::Operation_Binary>())	{
			const IR::Operation_Binary* obe = e->to<IR::Operation_Binary>();
			Value* left; Value* right;
			left = processExpression(obe->left, bbIf, bbElse);
			
			if(!(e->is<IR::LAnd>() || e->is<IR::LOr>()))	{
				right = processExpression(obe->right, bbIf, bbElse);
			}

			if(e->is<IR::Add>())	
				return Builder.CreateAdd(left,right);	

			if(e->is<IR::Sub>())	
				return Builder.CreateSub(left,right);	

			if(e->is<IR::Mul>())	
				return Builder.CreateMul(left,right);	

			if(e->is<IR::Div>())	
				return Builder.CreateSDiv(left,right);	

			if(e->is<IR::Mod>())
				return Builder.CreateSRem(left,right);	

			if(e->is<IR::Shl>())
				return Builder.CreateShl(left,right);	

			if(e->is<IR::Shr>())
				return Builder.CreateLShr(left,right);

			if(e->is<IR::BAnd>())
				return Builder.CreateAnd(left,right);
			
			if(e->is<IR::BOr>())
				return Builder.CreateOr(left,right);

			if(e->is<IR::BXor>())
				return Builder.CreateXor(left,right);

			if(e->is<IR::LAnd>())	{
	            BasicBlock* bbNext = BasicBlock::Create(TheContext, "land.rhs", function);								
	            BasicBlock* bbEnd;
				
				Value* icmp1;
				Value* icmp2;

				if(obe->left->is<IR::PathExpression>())	{
					icmp1 = Builder.CreateICmpNE(left,ConstantInt::get(left->getType(),0));
					bbEnd = BasicBlock::Create(TheContext, "land.end", function);
					Builder.CreateCondBr(icmp1, bbNext, bbEnd);				
				}
				else	{
					assert(bbElse != nullptr);
					icmp1 = left;
					Builder.CreateCondBr(icmp1, bbNext, bbElse);
				}
		        
				Builder.SetInsertPoint(bbNext);
				BasicBlock* bbParent = bbNext->getSinglePredecessor();
				assert(bbParent != nullptr);
				
				right = processExpression(obe->right, bbIf, bbElse);
				if(obe->right->is<IR::PathExpression>())	{
					icmp2 = Builder.CreateICmpNE(right,ConstantInt::get(right->getType(),0));	
					Builder.CreateBr(bbEnd);
				}
				else	{
					icmp2 = right;
					return icmp2;		
				}

				Builder.SetInsertPoint(bbEnd);
				PHINode* phi = Builder.CreatePHI(icmp1->getType(), 2);
				phi->addIncoming(ConstantInt::getFalse(TheContext), bbParent);
				phi->addIncoming(icmp2, bbNext);
				return phi;
			}

			if(e->is<IR::LOr>())	{
	            BasicBlock* bbTrue;
	            BasicBlock* bbFalse = BasicBlock::Create(TheContext, "land.rhs", function);
				
				Value* icmp1;
				Value* icmp2;
				
				if(obe->left->is<IR::PathExpression>())	{
					bbTrue = BasicBlock::Create(TheContext, "land.end", function);
					icmp1 = Builder.CreateICmpNE(left,ConstantInt::get(left->getType(),0));
					Builder.CreateCondBr(icmp1, bbTrue, bbFalse);
				}
				else	{
					assert(bbIf != nullptr);
					icmp1 = left;
					Builder.CreateCondBr(icmp1, bbIf, bbFalse);
				}
				Builder.SetInsertPoint(bbFalse);
				BasicBlock* bbParent = bbFalse->getSinglePredecessor();
				assert(bbParent != nullptr);
				
				right = processExpression(obe->right, bbIf, bbElse);
				if(obe->right->is<IR::PathExpression>())	{				
					icmp2 = Builder.CreateICmpNE(right,ConstantInt::get(right->getType(),0));
					Builder.CreateBr(bbTrue);
				}
				else	{
					icmp2 = right;
					return icmp2;
				}

				Builder.SetInsertPoint(bbTrue);
				PHINode* phi = Builder.CreatePHI(icmp1->getType(), 2);
				phi->addIncoming(ConstantInt::getTrue(TheContext), bbParent);
				phi->addIncoming(icmp2, bbFalse);
				return phi;
			}

			if(e->is<IR::Operation_Relation>())	{
				if(obe->right->is<IR::Constant>())
					right = processExpression(obe->right, bbIf, bbElse);

				if(e->is<IR::Equ>())
					return Builder.CreateICmpEQ(left,right);

				if(e->is<IR::Neq>())	
					return Builder.CreateICmpNE(left,right);
					
				if(e->is<IR::Lss>())	
					return Builder.CreateICmpSLT(left,right);
				
				if(e->is<IR::Leq>())	
					return Builder.CreateICmpSLE(left,right);

				if(e->is<IR::Grt>())	
					return Builder.CreateICmpSGT(left,right);

				if(e->is<IR::Geq>())	
					return Builder.CreateICmpSGE(left,right);
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

	}


    // Helper Function
    llvm::Type* EmitLLVMIR::getCorrespondingType(const IR::Type *t)
    {


        // Type_Name
        // Type_Enum

        std::cout << __LINE__ << std::endl;
        if(!t)
        {
            return nullptr;
        }
    	if(t->is<IR::Type_Boolean>())
    	{	
			llvm::Type *temp = Type::getInt8Ty(TheContext);							
			defined_type[t->toString()] = temp;
	    	return temp;
    	}

    	// if int<> or bit<> /// bit<32> x; /// The type of x is Type_Bits(32); /// The type of 'bit<32>' is Type_Type(Type_Bits(32))
    	else if(t->is<IR::Type_Bits>()) // Bot int <> and bit<> passes this check
    	{
            std::cout << __LINE__ << std::endl;
    		const IR::Type_Bits *x =  dynamic_cast<const IR::Type_Bits *>(t);
    		
    		int width = x->width_bits(); // false
    		// To do this --> How to implement unsigned and signed int in llvm
    		if(x->isSigned)
    		{	
				llvm::Type *temp = Type::getIntNTy(TheContext, getByteAlignment(width));				
				defined_type[t->toString()] = temp;
				return temp;
    		}
    		else 
    		{	
				llvm::Type *temp = Type::getIntNTy(TheContext, getByteAlignment(width));
				defined_type[t->toString()] = temp;
    			return temp;
    		}
    	}

    	else if (t->is<IR::Type_StructLike>()) 
    	{
            std::cout << __LINE__ << std::endl;
            // Just a bug check
    		if (!t->is<IR::Type_Struct>() && !t->is<IR::Type_Header>() && !t->is<IR::Type_HeaderUnion>())
    		{
    			MYDEBUG(std::cout<<"Error: Unknown Type : " << t->toString() << std::endl);
    			return(Type::getInt32Ty(TheContext)); 
    		}


            if(defined_type[t->toString()])
            {
                llvm::Type *x = defined_type[t->toString()];
                llvm::StructType *y = dyn_cast<llvm::StructType>(x);
                if(!y->isOpaque()) // if opaque then define it
                {
                    return(x);
                }
                // else define it (below)
            }

            const IR::Type_StructLike *strct = dynamic_cast<const IR::Type_StructLike *>(t);

    		// Vector to Store attributes(variables) of struct
			std::vector<Type*> members;
			for (auto f : strct->fields)
			{
    			members.push_back(getCorrespondingType(f->type));
			}

			llvm::Type *temp =llvm::StructType::get(TheContext, members);
			defined_type[t->toString()] = temp;
			return(temp);
    	}

    	//eg-> header Ethernet_t is refered by Ethernet_t (type name is Ethernet_t) 
    	// c++ equal => typedef struct x x --> As far as i understood
        else if(t->is<IR::Type_Typedef>())
        {
            std::cout << __LINE__ << std::endl;
            // MYDEBUG(std::cout << "Incomplete Function : "<<__LINE__ << " : Not Yet Implemented for " << t->toString()<< std::endl;)
            const IR::Type_Typedef *x = dynamic_cast<const IR::Type_Typedef *>(t);
            return(getCorrespondingType(x->type)); 
        }
		else if(t->is<IR::Type_Name>())
		{
            std::cout << __LINE__ << std::endl;
			if(defined_type[t->toString()])
			{
                std::cout << __LINE__ << std::endl;
				return(defined_type[t->toString()]);
			}
			// llvm::Type *temp = llvm::StructType::create(TheContext, "struct." + std::string(t->toString()));
            // FillIt(temp, t);
            CHECK_NULL(t);
            CHECK_NULL(typeMap);
            auto canon = typeMap->getTypeType(t, true);
            // auto canon = typeMap->getType(t);
            std::cout << __LINE__ << std::endl;
            defined_type[t->toString()] = getCorrespondingType(canon);
            std::cout << canon << ":" << defined_type[t->toString()] << std::endl;
            return (defined_type[t->toString()]);
		}
    	else if(t->is<IR::Type_Tuple>())
    	{
            std::cout << __LINE__ << std::endl;
    		MYDEBUG(std::cout << "Incomplete Function : "<<__LINE__ << " : Not Yet Implemented for " << t->toString()<< std::endl;)	
    	}
		else if(t->is<IR::Type_Void>())
		{
            std::cout << __LINE__ << std::endl;
			MYDEBUG(std::cout << "Incomplete Function : "<<__LINE__ << " : Not Yet Implemented for " << t->toString()<< std::endl;)
		}
		else if(t->is<IR::Type_Set>())
		{
            std::cout << __LINE__ << std::endl;
			MYDEBUG(std::cout << "Incomplete Function : "<<__LINE__ << " : Not Yet Implemented for " << t->toString()<< std::endl;)
		}
		else if(t->is<IR::Type_Stack>())
		{std::cout << __LINE__ << std::endl;
			MYDEBUG(std::cout << "Incomplete Function : "<<__LINE__ << " : Not Yet Implemented for " << t->toString()<< std::endl;)
		}
		else if(t->is<IR::Type_String>())
		{std::cout << __LINE__ << std::endl;
			MYDEBUG(std::cout << "Incomplete Function : "<<__LINE__ << " : Not Yet Implemented for " << t->toString()<< std::endl;)
		}

		else if(t->is<IR::Type_Table>())
		{std::cout << __LINE__ << std::endl;  
			MYDEBUG(std::cout << "Incomplete Function : "<<__LINE__ << " : Not Yet Implemented for " << t->toString()<< std::endl;)
		}
		else if(t->is<IR::Type_Varbits>())
		{std::cout << __LINE__ << std::endl;
			MYDEBUG(std::cout << "Incomplete Function : "<<__LINE__ << " : Not Yet Implemented for " << t->toString()<< std::endl;)
		}
        else if(t->is<IR::Type_Var>())
        {std::cout << __LINE__ << std::endl;        
            // t->getVarName()
            // t->getP4Type()

            MYDEBUG(std::cout << "Incomplete Function : "<<__LINE__ << " : Not Yet Implemented for " << t->toString() << std::endl;)
            // return(getCorrespondingType(t->getP4Type()));
        }
        else if(t->is<IR::Type_Enum>())
        {std::cout << __LINE__ << std::endl;
            const IR::Type_Enum *t_enum = dynamic_cast<const IR::Type_Enum *>(t);

            for(auto x: t_enum->members)
            {std::cout << __LINE__ << std::endl;
                std::cout << x->name << " " << x->declid << std::endl;
            }
            MYDEBUG(std::cout << "Incomplete Function : " <<__LINE__ << " : Not Yet Implemented for Type_Enum " << t->toString() << std::endl;)
        }
		else
		{
            std::cout << __LINE__ << std::endl;
			MYDEBUG(std::cout << "Incomplete Function : "<<__LINE__ << " : Not Yet Implemented for " << t->toString() << "==> getType ==> " << typeMap->getType(t) << std::endl;)
		}
		MYDEBUG(std::cout << "Returning Int32 pointer for Incomplete Type" << std::endl;)
    	return(Type::getInt32PtrTy(TheContext));
	}
    #define VECTOR_VISIT(V, T, SS)                                                      \
        bool EmitLLVMIR::preorder(const IR:: V <IR::T> *v){                             \
        std::cout<<"\n" << SS << "\t "<<*v<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";   \
        if (v == nullptr)                                                               \
            return true;                                                               \
        for (auto a : *v)                                                               \
        {                                                                               \
            visit(a);                                                                   \
        }                                                                               \
        return true; }


    VECTOR_VISIT(Vector, ActionListElement, "Vector::ActionListElement")
    VECTOR_VISIT(Vector, Annotation, "Vector<Annotation>")
    VECTOR_VISIT(Vector, Entry, "Vector<Entry>")
    VECTOR_VISIT(Vector, Expression, "Vector<Expression>")
    VECTOR_VISIT(Vector, KeyElement, "Vector<KeyElement>")
    VECTOR_VISIT(Vector, Method, "Vector<Method>")
    VECTOR_VISIT(Vector, Node, "(Vector<Node>")
    VECTOR_VISIT(Vector, SelectCase, "Vector<SelectCase>")
    VECTOR_VISIT(Vector, SwitchCase, "Vector<SwitchCase>")
    VECTOR_VISIT(Vector, Type, "Vector<Type>")
    VECTOR_VISIT(IndexedVector, Declaration, "IndexedVector<Declaration>")
    VECTOR_VISIT(IndexedVector, Declaration_ID, "IndexedVector<Declaration_ID>")
    VECTOR_VISIT(IndexedVector, Node, "IndexedVector<Node>")
    VECTOR_VISIT(IndexedVector, ParserState, "IndexedVector<ParserState>")
    VECTOR_VISIT(IndexedVector, StatOrDecl, "IndexedVector<StatOrDecl>")

    #undef VECTOR_VISIT


    // ______________________________________________________________________________________________________
    bool EmitLLVMIR::preorder(const IR::Type_Boolean* t)
     {
        std::cout<<"\nType_Boolean\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }
    bool EmitLLVMIR::preorder(const IR::Type_Varbits* t)
    {
        std::cout<<"\nType_Varbits\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }
    bool EmitLLVMIR::preorder(const IR::Type_Bits* t)
    {
        std::cout<<"\nType_Bits\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }
    bool EmitLLVMIR::preorder(const IR::Type_InfInt* t)
    {
        std::cout<<"\nType_InfInt\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }

    bool EmitLLVMIR::preorder(const IR::Type_Dontcare* t)
    {
        std::cout<<"\nType_Dontcare\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }
    bool EmitLLVMIR::preorder(const IR::Type_Void* t)
    {
        std::cout<<"\nype_Void\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }
    bool EmitLLVMIR::preorder(const IR::Type_Error* t)
    {
        std::cout<<"\nType_Error\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }


    bool EmitLLVMIR::preorder(const IR::Type_Name* t)
    {
        std::cout<<"\nType_Name\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;

    }

    bool EmitLLVMIR::preorder(const IR::Type_Package* t)
    {
        std::cout<<"\nType_Package\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }


    bool EmitLLVMIR::preorder(const IR::Type_Stack* t)
    {
        std::cout<<"\nType_Stack\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }
    bool EmitLLVMIR::preorder(const IR::Type_Specialized* t)
    {
        std::cout<<"\nType_Specialized\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }
    bool EmitLLVMIR::preorder(const IR::Type_Enum* t)
    {
        std::cout<<"\nType_Enum\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }
    bool EmitLLVMIR::preorder(const IR::Type_Typedef* t)
    {
        std::cout<<"\nType_Typedef\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }
    bool EmitLLVMIR::preorder(const IR::Type_Type* t)
    {
        std::cout<<"\nType_Type\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }
    bool EmitLLVMIR::preorder(const IR::Type_Unknown* t)
    {
        std::cout<<"\nType_Unknown\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }
    bool EmitLLVMIR::preorder(const IR::Type_Tuple* t)
    {
        std::cout<<"\nType_Tuple\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }

    // declarations
    bool EmitLLVMIR::preorder(const IR::Declaration_Constant* cst)
    {
        std::cout<<"\nDeclaration_Constant\t "<<*cst<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }
    bool EmitLLVMIR::preorder(const IR::Declaration_Instance* t)
    {
        std::cout<<"\nDeclaration_Instance\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }
    bool EmitLLVMIR::preorder(const IR::Declaration_MatchKind* t)
    {
        std::cout<<"\nDeclaration_MatchKind\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }

    // expressions
    bool EmitLLVMIR::preorder(const IR::Constant* t)
    {
        std::cout<<"\nConstant\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }
    bool EmitLLVMIR::preorder(const IR::Slice* t)
    {
        std::cout<<"\nSlice\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }
    bool EmitLLVMIR::preorder(const IR::BoolLiteral* t)
    {
        std::cout<<"\nBoolLiteral\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }
    bool EmitLLVMIR::preorder(const IR::StringLiteral* t)
    {
        std::cout<<"\nStringLiteral\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }
    bool EmitLLVMIR::preorder(const IR::PathExpression* t)
    {
        std::cout<<"\nPathExpression\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }
    bool EmitLLVMIR::preorder(const IR::Cast* t)
    {
        std::cout<<"\nCast\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }
    bool EmitLLVMIR::preorder(const IR::Operation_Binary* t)
    {
        std::cout<<"\nOperation_Binary\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }
    bool EmitLLVMIR::preorder(const IR::Operation_Unary* t)
    {
        std::cout<<"\nOperation_Unary\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }
    bool EmitLLVMIR::preorder(const IR::ArrayIndex* t)
    {
        std::cout<<"\nArrayIndex\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }
    bool EmitLLVMIR::preorder(const IR::TypeNameExpression* t)
    {
        std::cout<<"\nTypeNameExpression\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }
    bool EmitLLVMIR::preorder(const IR::Mux* t)
    {
        std::cout<<"\nMux\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }
    bool EmitLLVMIR::preorder(const IR::ConstructorCallExpression* t)
    {
        std::cout<<"\nConstructorCallExpression\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }
    bool EmitLLVMIR::preorder(const IR::Member* t)
    {
        std::cout<<"\nMember\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }
    bool EmitLLVMIR::preorder(const IR::SelectCase* t)
    {
        //handled inside SelectExpression
        std::cout<<"\nSelectCase\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }
    bool EmitLLVMIR::preorder(const IR::SelectExpression* t)
    {
        std::cout<<"\nSelectExpression\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        
        //right now, only supports single key based select. and cases are allowed only on integer constant expressions
        /*
        if (defined_state.find("start") == defined_state.end()) {
            defined_state["start"] = BasicBlock::Create(TheContext, "start", function);
        }
        */
        if (defined_state.find("reject") == defined_state.end()) {
            defined_state["reject"] = BasicBlock::Create(TheContext, "reject", function);
        }

        SwitchInst *sw = Builder.CreateSwitch(processExpression(t->select,NULL,NULL), defined_state["reject"], t->selectCases.size());
        //comment above line and uncomment below commented code to test selectexpression with dummy select key
        /*
        Value* tmp = ConstantInt::get(IntegerType::get(TheContext, 64), 1024, true);
        SwitchInst *sw = Builder.CreateSwitch(tmp, defined_state["reject"], t->selectCases.size());
        */

        bool issetdefault = false;
        for (int i=0;i<t->selectCases.size();i++) {
            //visit(t->selectCases[i]);
            if (dynamic_cast<const IR::DefaultExpression *>(t->selectCases[i]->keyset)) {
                if (issetdefault) continue;
                else {
                    issetdefault = true;
                    if (defined_state.find(t->selectCases[i]->state->path->asString()) == defined_state.end()) {
                        defined_state[t->selectCases[i]->state->path->asString()] = BasicBlock::Create(TheContext, *new const Twine(t->selectCases[i]->state->path->asString()), function);
                    }
                    sw->setDefaultDest(defined_state[t->selectCases[i]->state->path->asString()]);
                }
            }
            else if (dynamic_cast<const IR::Constant *>(t->selectCases[i]->keyset)) {
                if (defined_state.find(t->selectCases[i]->state->path->asString()) == defined_state.end()) {
                    defined_state[t->selectCases[i]->state->path->asString()] = BasicBlock::Create(TheContext, *new const Twine(t->selectCases[i]->state->path->asString()), function);
                }
                ConstantInt *onVal = ConstantInt::get(IntegerType::get(TheContext, 64), ((IR::Constant *)(t->selectCases[i]->keyset))->asLong(), true);

                sw->addCase(onVal, defined_state[t->selectCases[i]->state->path->asString()]);
            }
        }
        return false;
    }
    bool EmitLLVMIR::preorder(const IR::ListExpression* t)
    {
        std::cout<<"\nListExpression\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }
    bool EmitLLVMIR::preorder(const IR::MethodCallExpression* t)
    {
        std::cout<<"\nMethodCallExpression\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }
    bool EmitLLVMIR::preorder(const IR::DefaultExpression* t)
    {
        std::cout<<"\nDefaultExpression\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }
    bool EmitLLVMIR::preorder(const IR::This* t)
    {
        std::cout<<"\nThis\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }



    // // statements
    bool EmitLLVMIR::preorder(const IR::BlockStatement* t)
    {
        std::cout<<"\nBlockStatement\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }
    bool EmitLLVMIR::preorder(const IR::MethodCallStatement* t)
    {
        std::cout<<"\nMethodCallStatement\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }
    bool EmitLLVMIR::preorder(const IR::EmptyStatement* t)
    {
        std::cout<<"\nEmptyStatement\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }
    bool EmitLLVMIR::preorder(const IR::ReturnStatement* t)
    {
        std::cout<<"\nReturnStatement\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }
    bool EmitLLVMIR::preorder(const IR::ExitStatement* t)
    {
        std::cout<<"\nExitStatement\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }
    bool EmitLLVMIR::preorder(const IR::SwitchCase* t)
    {
        std::cout<<"\nSwitchCase\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }
    bool EmitLLVMIR::preorder(const IR::SwitchStatement* t)
    {
        std::cout<<"\nSwitchStatement\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }

    // misc
    bool EmitLLVMIR::preorder(const IR::Path* t)
    {
        std::cout<<"\nPath\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }
    bool EmitLLVMIR::preorder(const IR::Annotation* t)
    {
        std::cout<<"\nAnnotation\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }
    bool EmitLLVMIR::preorder(const IR::P4Program* t)
    {
        std::cout<<"\nP4Program\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }
    bool EmitLLVMIR::preorder(const IR::P4Action* t)
    {
        std::cout<<"\nP4Action\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }
    bool EmitLLVMIR::preorder(const IR::Method* t)
    {
        std::cout<<"\nMethod\t "<<t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }
    bool EmitLLVMIR::preorder(const IR::Function* t)
    {
        std::cout<<"\nFunction\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }

    bool EmitLLVMIR::preorder(const IR::ExpressionValue* t)
    {
        std::cout<<"\nExpressionValue\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }
    bool EmitLLVMIR::preorder(const IR::ActionListElement* t)
    {
        std::cout<<"\nActionListElement\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }
    bool EmitLLVMIR::preorder(const IR::ActionList* t)
    {
        std::cout<<"\nActionList\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }
    bool EmitLLVMIR::preorder(const IR::Key* t)
    {
        std::cout<<"\nKey\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }
    bool EmitLLVMIR::preorder(const IR::Property* t)
    {
        std::cout<<"\nProperty\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }
    bool EmitLLVMIR::preorder(const IR::TableProperties* t)
    {
        std::cout<<"\nTableProperties\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }
    bool EmitLLVMIR::preorder(const IR::EntriesList *t)
    {
        std::cout<<"\nEntriesList\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }
    bool EmitLLVMIR::preorder(const IR::Entry *t)
    {
        std::cout<<"\nEntry\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }
    bool EmitLLVMIR::preorder(const IR::P4Table* t)
    {
        std::cout<<"\nP4Table\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }

    // in case it is accidentally called on a V1Program
    bool EmitLLVMIR::preorder(const IR::V1Program* t)
    {
        std::cout<<"\nV1Program\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
        
    }
}