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


    bool EmitLLVMIR::preorder(const IR::P4Parser* t)
    {
        // Parser have apply methods
        // MYDEBUG(std::cout<< t->)
        for (auto s : t->states)
        {
            llvm::BasicBlock* bbInsert = llvm::BasicBlock::Create(TheContext, std::string(s->name.name), function);
            defined_state[s->name.name] = bbInsert;
            MYDEBUG(std::cout << s->name.name << std::endl;);
        }
        for (auto s : t->states)
        {
            visit(s);
        }
        Builder.SetInsertPoint(defined_state["accept"]);
        return false;
    }

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
            return false;

        }
        else if(parserState->name == "reject")
        {
            Builder.CreateRetVoid();
            return false;
        }
        

        // visit all the StatOrDecl
        for(auto s: parserState->components)
        {
            visit(s, "components");// or class helper function if defined
        }

        // if  select expression is null
        // Create branch to reject(there should be some transition)
        if (parserState->selectExpression == nullptr)
        {
            MYDEBUG(std::cout<< parserState->selectExpression << std::endl;);
            Builder.CreateBr(defined_state["reject"]); // if reject then return 0;
        } 
        // visit SelectExpression
        else if (parserState->selectExpression->is<IR::SelectExpression>())
        {
            visit(parserState->selectExpression, "PathExpression");
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
        return false;
    }
    






    // bool EmitLLVMIR::preorder(const IR::P4Control* t)
    // {
    //     MYDEBUG(std::cout << t->type << std::endl;);
    //     llvm::BasicBlock* bbInsert = llvm::BasicBlock::Create(TheContext, "control_block", function);
    //     // visit(t->type, "Type_Control");
    //     return false;
    // }






	bool EmitLLVMIR::preorder(const IR::AssignmentStatement* t)
    {
        std::cout<<"\nAssignmentStatement\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
		llvm::Type* llvmType = nullptr;

		if(t->left->is<IR::PathExpression>())	{
			cstring name = refMap->getDeclaration(t->left->to<IR::PathExpression>()->path)->getName();
			std::cout<<"name using refmap = "<<name<<std::endl;		
			Value* v = st.lookupLocal("alloca_"+name);

			assert(v != nullptr);

			llvmType = defined_type[typeMap->getType(t->left)->toString()];
			std::cout<<"defined_type["<< typeMap->getType(t->left)->toString()<<"]\n";
			std::cout<<"t->left->tostring = "<<t->left->toString()<<"\n";
			assert(llvmType != nullptr);

			Value* right = processExpression(t->right, llvmType);
			
			if(right != nullptr)	{
				//store 
				Builder.CreateStore(right,v);			
			}
			
		}

		// else	{
		// 	//To-Do
		// }
        
        return true;
    }

	Value* EmitLLVMIR::processExpression(const IR::Expression* e, llvm::Type* type)	{
		assert(e != nullptr);
		assert(type != nullptr);
		
		llvm::Type* llvmType = nullptr;

		if(e->is<IR::Operation_Unary>())	{
			const IR::Operation_Unary* oue = e->to<IR::Operation_Unary>();
			Value* exp = processExpression(oue->expr, type);
			if(e->is<IR::Cmpl>()) 
				return Builder.CreateXor(exp,-1);

			if(e->is<IR::Neg>())	
				return Builder.CreateSub(ConstantInt::get(type,0), exp);

			//if(e->is<IR::Not>())
				//Not required as statements are converted automatically to its negated form.
				//eg: !(a==b) is converted as a!=b by other passes.
		}

		if(e->is<IR::Constant>())   
			return ConstantInt::get(type,(e->to<IR::Constant>()->value).get_si());

		if(e->is<IR::PathExpression>())	{
			cstring name = refMap->getDeclaration(e->to<IR::PathExpression>()->path)->getName();	
			Value* v = st.lookupLocal("alloca_"+name);
			assert(v != nullptr);
			return Builder.CreateLoad(v);
		}

		if(e->is<IR::Operation_Binary>())	{
			const IR::Operation_Binary* obe = e->to<IR::Operation_Binary>();
			Value* left; Value* right;
			left = processExpression(obe->left, type);
			
			if(!(e->is<IR::LAnd>() || e->is<IR::LOr>()))	{
				right = processExpression(obe->right, type);
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
	            BasicBlock* bbTrue = BasicBlock::Create(TheContext, "land.rhs", function);
	            BasicBlock* bbFalse = BasicBlock::Create(TheContext, "land.end", function);
				
				Value* icmp1 = Builder.CreateICmpNE(left,ConstantInt::get(type,0));
				Builder.CreateCondBr(icmp1, bbTrue, bbFalse);
		        
				Builder.SetInsertPoint(bbTrue);
				BasicBlock* bbParent = bbTrue->getSinglePredecessor();
				assert(bbParent != nullptr);
				
				right = processExpression(obe->right, type);
				Value* icmp2 = Builder.CreateICmpNE(right,ConstantInt::get(type,0));
				Builder.CreateBr(bbFalse);

				Builder.SetInsertPoint(bbFalse);
				PHINode* phi = Builder.CreatePHI(icmp1->getType(), 2);
				phi->addIncoming(ConstantInt::getFalse(TheContext), bbParent);
				phi->addIncoming(icmp2, bbTrue);
				return Builder.CreateZExt(phi, type);
			}

			if(e->is<IR::LOr>())	{
	            BasicBlock* bbTrue = BasicBlock::Create(TheContext, "land.end", function);
	            BasicBlock* bbFalse = BasicBlock::Create(TheContext, "land.rhs", function);
				
				Value* icmp1 = Builder.CreateICmpNE(left,ConstantInt::get(type,0));
				Builder.CreateCondBr(icmp1, bbTrue, bbFalse);
		        
				Builder.SetInsertPoint(bbFalse);
				BasicBlock* bbParent = bbFalse->getSinglePredecessor();
				assert(bbParent != nullptr);
				
				right = processExpression(obe->right, type);
				Value* icmp2 = Builder.CreateICmpNE(right,ConstantInt::get(type,0));
				Builder.CreateBr(bbTrue);

				Builder.SetInsertPoint(bbTrue);
				PHINode* phi = Builder.CreatePHI(icmp1->getType(), 2);
				phi->addIncoming(ConstantInt::getTrue(TheContext), bbParent);
				phi->addIncoming(icmp2, bbTrue);
				return Builder.CreateZExt(phi, type);
			}

			if(e->is<IR::Operation_Relation>())	{
				if(obe->right->is<IR::Constant>())
					right = processExpression(obe->right, defined_type[typeMap->getType(obe->left)->toString()]);

				if(e->is<IR::Equ>())
					return Builder.CreateZExt(Builder.CreateICmpEQ(left,right), type);

				if(e->is<IR::Neq>())	
					return Builder.CreateZExt(Builder.CreateICmpNE(left,right), type);
					
				if(e->is<IR::Lss>())	
					return Builder.CreateZExt(Builder.CreateICmpSLT(left,right), type);
				
				if(e->is<IR::Leq>())	
					return Builder.CreateZExt(Builder.CreateICmpSLE(left,right), type);

				if(e->is<IR::Grt>())	
					return Builder.CreateZExt(Builder.CreateICmpSGT(left,right), type);

				if(e->is<IR::Geq>())	
					return Builder.CreateZExt(Builder.CreateICmpSGE(left,right), type);
			}
				
		}
	}


    // Helper Function
    llvm::Type* EmitLLVMIR::getCorrespondingType(const IR::Type *t)
    {

    	if(t->is<IR::Type_Boolean>())
    	{	
			llvm::Type *temp = Type::getInt8Ty(TheContext);							
			defined_type[t->toString()] = temp;
	    	return temp;
    	}

    	// if int<> or bit<> /// bit<32> x; /// The type of x is Type_Bits(32); /// The type of 'bit<32>' is Type_Type(Type_Bits(32))
    	else if(t->is<IR::Type_Bits>()) // Bot int <> and bit<> passes this check
    	{
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
    		if(defined_type[t->toString()])
    		{
    			llvm::Type *x = defined_type[t->toString()];
                llvm::StructType *y = dyn_cast<llvm::StructType>(x);
                if(!y->isOpaque()) // if opaque then define it
                {
        			return(x);
                }
                MYDEBUG(std::cout<<" isOpaque : True for " << t->toString() << std::endl);
            }
    		const IR::Type_StructLike *strct = dynamic_cast<const IR::Type_StructLike *>(t);
    		if (!t->is<IR::Type_Struct>() && !t->is<IR::Type_Header>() && !t->is<IR::Type_HeaderUnion>())
    		{
    			MYDEBUG(std::cout<<"Error: Unknown Type : " << t->toString() << std::endl);
    			return(Type::getInt32Ty(TheContext)); 
    		}

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
            // MYDEBUG(std::cout << "Incomplete Function : "<<__LINE__ << " : Not Yet Implemented for " << t->toString()<< std::endl;)
            const IR::Type_Typedef *x = dynamic_cast<const IR::Type_Typedef *>(t);
            return(getCorrespondingType(x->type)); 
        }
		else if(t->is<IR::Type_Name>())
		{
			if(defined_type[t->toString()])
			{
				return(defined_type[t->toString()]);
			}
			// llvm::Type *temp = llvm::StructType::create(TheContext, "struct." + std::string(t->toString()));
            // FillIt(temp, t);
            auto canon = typeMap->getTypeType(t, true);
            defined_type[t->toString()] = getCorrespondingType(canon);
            return (defined_type[t->toString()]);
		}
    	else if(t->is<IR::Type_Tuple>())
    	{
    		MYDEBUG(std::cout << "Incomplete Function : "<<__LINE__ << " : Not Yet Implemented for " << t->toString()<< std::endl;)	
    	}
		else if(t->is<IR::Type_Void>())
		{
			MYDEBUG(std::cout << "Incomplete Function : "<<__LINE__ << " : Not Yet Implemented for " << t->toString()<< std::endl;)
		}
		else if(t->is<IR::Type_Set>())
		{
			MYDEBUG(std::cout << "Incomplete Function : "<<__LINE__ << " : Not Yet Implemented for " << t->toString()<< std::endl;)
		}
		else if(t->is<IR::Type_Stack>())
		{
			MYDEBUG(std::cout << "Incomplete Function : "<<__LINE__ << " : Not Yet Implemented for " << t->toString()<< std::endl;)
		}
		else if(t->is<IR::Type_String>())
		{
			MYDEBUG(std::cout << "Incomplete Function : "<<__LINE__ << " : Not Yet Implemented for " << t->toString()<< std::endl;)
		}

		else if(t->is<IR::Type_Table>())
		{
			MYDEBUG(std::cout << "Incomplete Function : "<<__LINE__ << " : Not Yet Implemented for " << t->toString()<< std::endl;)
		}
		else if(t->is<IR::Type_Varbits>())
		{
			MYDEBUG(std::cout << "Incomplete Function : "<<__LINE__ << " : Not Yet Implemented for " << t->toString()<< std::endl;)
		}
        else if(t->is<IR::Type_Enum>())
        {
            const IR::Type_Enum *t_enum = dynamic_cast<const IR::Type_Enum *>(t);

            for(auto x: t_enum->members)
            {
                std::cout << x->name << " " << x->declid << std::endl;
            }
            MYDEBUG(std::cout << "Incomplete Function : " <<__LINE__ << " : Not Yet Implemented for Type_Enum " << t->toString() << std::endl;)
        }
		else
		{
			MYDEBUG(std::cout << "Incomplete Function : "<<__LINE__ << " : Not Yet Implemented for " << t->toString() << "==> getTypeType ==> " << typeMap->getTypeType(t, true) << std::endl;)
		}
		MYDEBUG(std::cout << "Returning Int64 for Incomplete Type" << std::endl;)
    	return(Type::getInt64Ty(TheContext));
	}
}