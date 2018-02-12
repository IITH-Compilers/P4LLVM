#include "emitLLVMIR.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/DerivedTypes.h"

namespace P4	{
	
    cstring EmitLLVMIR::parseType(const IR::Type* p4Type)  {
        int width = p4Type->width_bits();
        MYDEBUG(std::cout<<"width = "<<width<<"\n";)
        int digitsInWidth = 0;
        while(width != 0)
        {
            width /= 10;
            ++digitsInWidth;
        }
        // std::cout<<"width = "<<width<<"\n";
        MYDEBUG(std::cout<<"digitsinwidth = "<<digitsInWidth<<"\n";)
        if(digitsInWidth == 0)
            return p4Type->toString();
        cstring type = p4Type->toString().substr(0,p4Type->toString().size()-(digitsInWidth+2));
        MYDEBUG(std::cout<<"parsed type is = "<<type;)
        return type;
    }

    unsigned EmitLLVMIR::getByteAlignment(unsigned width) {
        unsigned remainder = width % 8;
        unsigned newWidth = 0;
        if (remainder == 0)
            newWidth = width;
        else
            newWidth = width + 8 - remainder;
        return newWidth/8;
    }



    bool EmitLLVMIR::preorder(const IR::Declaration_Variable* t)
    {
        MYDEBUG(std::cout<<"name of variable = "<<t->getName()<<"\n";)
	    AllocaInst *alloca = Builder.CreateAlloca(getCorrespondingType(typeMap->getType(t)));
        st.insert("alloca_"+t->getName(),alloca);
        std::cout<<"\nDeclaration_Variable\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;
       return true;
    }
       



    // Need to create a map of struct name to its type pointer first (for struct of struct defination)
    bool EmitLLVMIR::preorder(const IR::Type_Struct* t)
    {
    	std::cout<<"\nType_Struct\t "<<*t << "--> " << typeMap->getType(t) <<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
    	
    	AllocaInst* alloca;

    	std::vector<Type*> members;
    	for(auto x: t->fields)
    	{
    		MYDEBUG(std::cout << __LINE__ << ":" << x->type << std::endl;);
    		// if()
	    	members.push_back(getCorrespondingType(x->type)); // for int and all
	    	// members.push_back(llvm::StructType::get(TheContext, members, "struct.A")) //  for struct of struct
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
    	cstring p4Type;
    	int width = typeMap->getType(t)->width_bits();
	    p4Type = typeMap->getType(t)->toString().substr(0, typeMap->getType(t)->toString().size()-1);
	    p4Type = p4Type.substr(5, p4Type.size());	    
    	MYDEBUG(std::cout << "\np4Type: " << p4Type << std::endl;)
    	unsigned alignment = getByteAlignment(typeMap->getType(t)->width_bits());
    	if(t->toString() == "bool")    {
    	    return(Type::getInt8Ty(TheContext));        
    	}
    	else if(p4Type == "bit") {
    	    return(ArrayType::get(Type::getInt8Ty(TheContext),alignment));
    	}
    	else if(p4Type == "int") {
    	    int width = alignment*8;
    	
    	    if(width==1) {               
    	       return(Type::getInt1Ty(TheContext));
    	    }
    	    else if(width==8) {                                  
    	        return(Type::getInt8Ty(TheContext));
    	    }
    	    else if(width==16) {
    	        return(Type::getInt16Ty(TheContext));
    	    }
    	    else if(width==32) {
    	        return(Type::getInt32Ty(TheContext));
    	    }
    	    else if(width==64){
    	        return(Type::getInt64Ty(TheContext));
    	    }
    	    else if(width==128){
    	        return(Type::getInt128Ty(TheContext));
    	    }

   		}
   		else if (p4Type == "struct") {
   			auto struct_pointer = defined_type[p4Type];
   			if(struct_pointer != NULL)
   			{
	   			return(struct_pointer);
   			}
   			else
   			{

   				std::vector<Type*> members;
		    	// Iterate over all struct attributes
		    	const IR::Type_Struct *y = dynamic_cast<const IR::Type_Struct *>(t);
		    	for(auto x: y->fields)
		    	{
		    		MYDEBUG(std::cout << __LINE__ << ":" << x->type << std::endl;);
			    	members.push_back(getCorrespondingType(x->type)); // for struct variable
		    	}
		    	return(llvm::StructType::get(TheContext, members));
   			}
   		}
   		else {
   			MYDEBUG(std::cout << __FILE__ << ":" << __LINE__ << ": Not Yet Implemented(Pointer returned =  Int32 for now) - Pankaj\n";)
   			return(Type::getInt32Ty(TheContext));
   		}
	}
}