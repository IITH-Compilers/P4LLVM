#include "emitLLVMIR.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"


namespace P4	{
	
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

    unsigned EmitLLVMIR::getByteAlignment(unsigned width) {
        unsigned remainder = width % 8;
        unsigned newWidth = 0;
        if (remainder == 0)
            newWidth = width;
        else
            newWidth = width + 8 - remainder;
        return newWidth/8;
    }



    bool EmitLLVMIR::preorder(const IR::Declaration_Variable* t) {
        std::cout<<"name of variable = "<<t->getName()<<"\n";
        cstring p4Type = parseType(typeMap->getType(t));
        unsigned alignment = getByteAlignment(typeMap->getType(t)->width_bits());
        AllocaInst* alloca;
        if(t->type->toString() == "bool")    {
             alloca = Builder.CreateAlloca(Type::getInt8Ty(TheContext));        
        }
        else if(p4Type == "bit") {
            alloca = Builder.CreateAlloca(ArrayType::get(Type::getInt8Ty(TheContext),alignment));
        }
        else if(p4Type == "int") {
            int width = alignment*8;
 
            if(width==1) {               
                alloca = Builder.CreateAlloca(Type::getInt1Ty(TheContext));
            }
            else if(width==8) {                                  
                alloca = Builder.CreateAlloca(Type::getInt8Ty(TheContext));
            }
            else if(width==16) {
                alloca = Builder.CreateAlloca(Type::getInt16Ty(TheContext));
            }
            else if(width==32) {
                alloca = Builder.CreateAlloca(Type::getInt32Ty(TheContext));
            }
            else if(width==64){
                alloca = Builder.CreateAlloca(Type::getInt64Ty(TheContext));
            }
            else if(width==128){
                alloca = Builder.CreateAlloca(Type::getInt128Ty(TheContext));
            }
        }

        st.insert("alloca_"+t->getName(),alloca);
        std::cout<<"\nDeclaration_Variable\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;

       return true;
    }

}