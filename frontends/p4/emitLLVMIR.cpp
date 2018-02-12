#include "emitLLVMIR.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/DerivedTypes.h"

namespace P4	{
	

	// Check this later
    unsigned EmitLLVMIR::getByteAlignment(unsigned width) {
    	if (width <= 8)
    	    return 1;
    	else if (width <= 16)
    	    return 2;
    	else if (width <= 32)
    	    return 4;
    	else if (width <= 64)
    	    return 8;
    	else
    	    // compiled as u8*
    	    return 1;
    }



    bool EmitLLVMIR::preorder(const IR::Declaration_Variable* t)
    {
    	MYDEBUG(std::cout<<"name of variable = "<<t->getName()<<"\n";)
	    AllocaInst *alloca = Builder.CreateAlloca(getCorrespondingType(typeMap->getType(t)));
    	st.insert("alloca_"+t->getName(),alloca);
    	std::cout<<"\nDeclaration_Variable\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;
    	return true;
    }

    cstring EmitLLVMIR::parseType(const IR::Type* p4Type)  {
        int width = p4Type->width_bits();
        std::cout<<"width = "<<width<<"\n";
        int digitsInWidth = 0;
        while(width != 0)
        {
            width /= 10;
            ++digitsInWidth;
        }
        // std::cout<<"width = "<<width<<"\n";
        std::cout<<"digitsinwidth = "<<digitsInWidth<<"\n";
        if(digitsInWidth == 0)
            return p4Type->toString();
        cstring type = p4Type->toString().substr(0,p4Type->toString().size()-(digitsInWidth+2));
        std::cout<<"parsed type is = "<<type;
        return type;
    }

    



    // Need to create a map of struct name to its type pointer first (for struct of struct defination)
    bool EmitLLVMIR::preorder(const IR::Type_Struct* t)
    {
    	std::cout<<"\nType_Struct\t "<<*t << "\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
    	
    	AllocaInst* alloca;
    	// Declare struct type
    	std::vector<Type*> members;
    	for(auto x: t->fields)
    	{
    		MYDEBUG(std::cout << __LINE__ << ":" << x->type << std::endl;);
	    	members.push_back(getCorrespondingType(x->type)); // for int and all
    	}
    	MYDEBUG(std::cout << __LINE__ << "\n";)
    	StructType *structReg = llvm::StructType::create(TheContext, members, "struct."+t->name); // 
    	alloca = Builder.CreateAlloca(structReg);
    	st.insert("alloca_"+t->getName(),alloca);
    	return true;
    }

    // Helper Function
    llvm::Type* EmitLLVMIR::getCorrespondingType(const IR::Type *t)
    {
    	cstring p4Type = parseType(typeMap->getType(t));



    	if(t->is<IR::Type_Boolean>())
    	{
	    	MYDEBUG(std::cout << __LINE__ << ": Type was here Type_Boolean" << "\n";)	
    		return(Type::getInt8Ty(TheContext));
    	}
    	// if int<> or bit<>
    	/// bit<32> x;
    	/// The type of x is Type_Bits(32);
    	/// The type of 'bit<32>' is Type_Type(Type_Bits(32))
    	else if(t->is<IR::Type_Bits>()) // Bot int <> and bit<> passes this check
    	{
    		const IR::Type_Bits *x =  dynamic_cast<const IR::Type_Bits *>(t);
    		MYDEBUG(std::cout << __LINE__ << ": Type was here Type_Bits" << "\n";) // Passes
    		int width = x->width_bits(); // false
    		if(x->isSigned)
    		{
    			// is_unsigned = 0; // this is for Bit
				return(Type::getIntNTy(TheContext, getByteAlignment(width)));
    		}
    		else 
    		{
    			// is_unsigned = 1;  // 1 is for int
    			return(Type::getIntNTy(TheContext, getByteAlignment(width)));
    		}
    	}


    	// ToDo --> Remember Declared type
    	else if (t->is<IR::Type_StructLike>()) 
    	{
    		const IR::Type_StructLike *strct = dynamic_cast<const IR::Type_StructLike *>(t);
    		MYDEBUG(std::cout << __LINE__ << ": Type was here - Type_StructLike" << "\n";)
    		if (t->is<IR::Type_Struct>())
    		{
    			MYDEBUG(std::cout << __LINE__ << ": Type was here - Type_StructLike - Type_Struct" << "\n";)
    		}
    		else if (t->is<IR::Type_Header>())
    		{
    			MYDEBUG(std::cout << __LINE__ << ": Type was here - Type_StructLike - Type_Header" << "\n";)
    		}
    		else if (t->is<IR::Type_HeaderUnion>())
    		{
    			MYDEBUG(std::cout << __LINE__ << ": Type was here - Type_StructLike - Type_HeaderUnion" << "\n";)
    		}
    		else
    		{
    			MYDEBUG(std::cout << __LINE__ << " : Undefined Type of " << t << " Returning INT32 for now\n";)
    			return(Type::getInt32Ty(TheContext)); 
    		}

    		// To Store attributes of struct
			std::vector<Type*> members;
			for (auto f : strct->fields)
			{
    			members.push_back(getCorrespondingType(f->type));
			}
			MYDEBUG(std::cout << __LINE__ << " : Type was here  Type_StructLike - Returning " << std::endl;);
			return(llvm::StructType::get(TheContext, members));
    	}






    	else if(t->is<IR::Type_Tuple>())
    	{
    		if (t->is<IR::Type_Struct>()) 
    		{
    			MYDEBUG(std::cout << __LINE__ << ": Type was here Type_Tuple-Type_Struct " << "\n";)
    		}
			else if(t->is<IR::Type_Header>())
			{
				MYDEBUG(std::cout << __LINE__ << ": Type was here Type_Tuple-Type_Header" << "\n";)
			}
			else 
			{
				MYDEBUG(std::cout << __LINE__ << ": Type was here Type_Tuple" << "\n";)

			}
    	}

		else if(t->is<IR::Type_Header>())
		{
			MYDEBUG(std::cout << __LINE__ << "Type was here" << "\n";)
		}
		else if(t->is<IR::Type_Void>())
		{
			MYDEBUG(std::cout << __LINE__ << "Type was here" << "\n";)
		}
		else if(t->is<IR::Type_HeaderUnion>())
		{
			MYDEBUG(std::cout << __LINE__ << "Type was here" << "\n";)
		}
		else if(t->is<IR::Type_Set>())
		{
			MYDEBUG(std::cout << __LINE__ << "Type was here" << "\n";)
		}
		else if(t->is<IR::Type_Name>())
		{
			// Implement it as struct -> this is refered by type
 ;
			auto x = typeMap->getTypeType(t, true);
    		if (x->is<IR::Type_Struct>()) 
    		{
    			MYDEBUG(std::cout << __LINE__ << ": Type was here Type_Tuple-Type_Struct " << "\n";)
    		}
			else if(x->is<IR::Type_Header>())
			{
				MYDEBUG(std::cout << __LINE__ << ": Type was here Type_Tuple-Type_Header" << "\n";)
			}
			else 
			{
				MYDEBUG(std::cout << __LINE__ << ": Type was here Type_Tuple" << "\n";)

			}
			MYDEBUG(std::cout << __LINE__  <<  "Type was here" << "\n";)
		}
		else if(t->is<IR::Type_Stack>())
		{
			MYDEBUG(std::cout << __LINE__ << "Type was here" << "\n";)
		}
		else if(t->is<IR::Type_String>())
		{
			MYDEBUG(std::cout << __LINE__ << "Type was here" << "\n";)
		}

		else if(t->is<IR::Type_Table>())
		{
			MYDEBUG(std::cout << __LINE__ << "Type was here" << "\n";)
		}
		else
		{
			MYDEBUG(std::cout << __LINE__ << "Type was here" << " : " <<  "\n";)
		}







     //    unsigned alignment = getByteAlignment(typeMap->getType(t)->width_bits());
     //    AllocaInst* alloca;
    	// if(t->is<IR::Bit>())    {
    	//     return(Type::getInt8Ty(TheContext));    
    	// }
    	// else if(p4Type == "bit") {
    	//     return(ArrayType::get(Type::getInt8Ty(TheContext),alignment));
    	// }
    	// else if(p4Type == "int") {
    	//     int width = alignment*8;
    	
    	//     if(width==1) {    
    	//     return(Type::getInt1Ty(TheContext));
    	//     }
    	//     else if(width==8) {    
    	//     return(Type::getInt8Ty(TheContext));
    	//     }
    	//     else if(width==16) {
    	//     return(Type::getInt16Ty(TheContext));
    	//     }
    	//     else if(width==32) {
    	//     return(Type::getInt32Ty(TheContext));
    	//     }
    	//     else if(width==64){
    	    return(Type::getInt64Ty(TheContext));
    	//     }
    	//     else if(width==128){
    	//     return(Type::getInt128Ty(TheContext));
    	//     }

   		// }
   		// else if (p4Type == "struct") {
   		// 	auto struct_pointer = defined_type[p4Type];
   		// 	if(struct_pointer != NULL)
   		// 	{
	   	// 		return(struct_pointer);
   		// 	}
   		// 	else
   		// 	{

   		// 		std::vector<Type*> members;
		   //  	// Iterate over all struct attributes
		   //  	const IR::Type_Struct *y = dynamic_cast<const IR::Type_Struct *>(t);
		   //  	for(auto x: y->fields)
		   //  	{
		   //  		MYDEBUG(std::cout << __LINE__ << ":" << x->type << std::endl;);
			  //   	members.push_back(getCorrespondingType(x->type)); // for struct variable
		   //  	}
		   //  	defined_type[p4Type] = llvm::StructType::get(TheContext, members);
		   //  	MYDEBUG(std::cout << __LINE__ << defined_type[p4Type] << std::endl;)
		   //  	return(llvm::StructType::get(TheContext, members));
   		// 	}
   		// }
   		// else {
   		// 	MYDEBUG(std::cout << __FILE__ << ":" << __LINE__ << ": Assuming Header Type - Pankaj\n";)
   		// 	auto struct_pointer = defined_type[p4Type];
   		// 	MYDEBUG(std::cout << __LINE__ << std::endl;)
   		// 	if(struct_pointer != NULL)
   		// 	{
	   	// 		return(struct_pointer);
   		// 	}
   		// 	else
   		// 	{
   		// 		MYDEBUG(std::cout << __LINE__ << std::endl;)
   		// 		std::vector<Type*> members;
		   //  	// Iterate over all struct attributes
		   //  	const IR::Type_Header *y = dynamic_cast<const IR::Type_Header *>(t);
		   //  	MYDEBUG(std::cout << __LINE__ << std::endl;)
		   //  	for(auto x: y->fields)
		   //  	{
		   //  		MYDEBUG(std::cout << __LINE__ << ":" << x->type << std::endl;)
			  //   	members.push_back(getCorrespondingType(x->type)); // for struct variable
		   //  	}
		   //  	MYDEBUG(std::cout << __LINE__ << std::endl;)
		   //  	return(llvm::StructType::get(TheContext, members));
   		// 	}
   		// }
	}
}