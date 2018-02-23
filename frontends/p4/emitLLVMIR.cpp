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
		// std::cout<<"t->name.name = "<< t->name.name << "t->name = "<<t->name<<"\nt->getname = "<<t->getName()<<"\n t->tostring() = "<<t->toString()<<"\n ";
		// std::cout<<" t->externalName() = "<<t->externalName()<<"\n  t->getName().name-> = "<<t->getName().name<<"\n   t->controlplanename-> = "<<t->controlPlaneName()<<"\n ";		
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

	bool EmitLLVMIR::preorder(const IR::AssignmentStatement* t) {
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
		// generateAssignmentStatements(t->left,t->right);
        std::cout<<"\nAssignmentStatement\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        return true;
    }

	Value* EmitLLVMIR::processExpression(const IR::Expression* e, llvm::Type* type)	{
		assert(e != nullptr);
		assert(type != nullptr);
		
		llvm::Type* llvmType = nullptr;

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
			Value* left = processExpression(obe->left, type);
			Value* right = processExpression(obe->right, type);

			if(e->is<IR::Add>())	
				return Builder.CreateAdd(left,right);	

			if(e->is<IR::Sub>())	
				return Builder.CreateSub(left,right);	

			if(e->is<IR::Mul>())	
				return Builder.CreateMul(left,right);	

			if(e->is<IR::Div>())	
				return Builder.CreateUDiv(left,right);	

			if(e->is<IR::Mod>())
				return Builder.CreateURem(left,right);	

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

			if(e->is<IR::Operation_Relation>());
				//To-Do
		}

		if(e->is<IR::Operation_Unary>())	{
			//To-Do
			// const IR::Operation_Unary* oue = e->to<IR::Operation_Unary>();
			// Value* exp = processExpression(oue->expr, type);

			// if(e->is<IR::Neg>())
			// 	return Builder.
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
    			return(x);
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
		else if(t->is<IR::Type_Name>())
		{
			if(defined_type[t->toString()])
			{
				return(defined_type[t->toString()]);
			}
			llvm::Type *temp = llvm::StructType::create(TheContext, "struct." + std::string(t->toString()));
			defined_type[t->toString()] = temp;
			return(temp);
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
		else
		{
			MYDEBUG(std::cout << "Incomplete Function : "<<__LINE__ << " : Not Yet Implemented for " << t->toString()<< std::endl;)
		}
		MYDEBUG(std::cout << "Returning Int64 for Incomplete Type" << std::endl;)
    	return(Type::getInt64Ty(TheContext));
	}

    // void buildStoreInst(Type* type, Type* oldType=nullptr)    {
    //     switch(type->getTypeID())  {
    //         case Type::TypeID::IntegerTyID :
    //             if(oldType != nullptr && oldType->getTypeID()==Type::TypeID::ArrayTyID && oldType->get)  {

    //             }

    //             Builder.CreateStore(ConstantInt::get(type,(con_literal->value).get_ui()),v);                
    //             std::cout<<"found type to be integer - "<<type->getIntegerBitWidth()<<std::endl;
    //         case Type::TypeID::ArrayTyID :
    //             if(type->getArrayNumElements() == 1)
    //             // buildStoreInst


    //     }
    // }

}