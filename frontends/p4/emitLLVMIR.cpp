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


    // Helper Function
    llvm::Type* EmitLLVMIR::getCorrespondingType(const IR::Type *t)
    {

    	if(t->is<IR::Type_Boolean>())
    	{
	    	return(Type::getInt8Ty(TheContext));
    	}

    	// if int<> or bit<> /// bit<32> x; /// The type of x is Type_Bits(32); /// The type of 'bit<32>' is Type_Type(Type_Bits(32))
    	else if(t->is<IR::Type_Bits>()) // Bot int <> and bit<> passes this check
    	{
    		const IR::Type_Bits *x =  dynamic_cast<const IR::Type_Bits *>(t);
    		
    		int width = x->width_bits(); // false
    		// To do this --> How to implement unsigned and signed int in llvm
    		if(x->isSigned)
    		{
				return(Type::getIntNTy(TheContext, getByteAlignment(width)));
    		}
    		else 
    		{
    			return(Type::getIntNTy(TheContext, getByteAlignment(width)));
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
}