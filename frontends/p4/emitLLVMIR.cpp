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
        std::cout<<"\nType of decl var = "<<t->type<<"\n";
        std::cout<<"\getP4Type() = "<<t->type->getP4Type()<<"\n";
        std::cout<<"\n*getP4Type() = "<<*t->type->getP4Type()<<"\n";
        std::cout<<"\ntypemap contains? = "<<typeMap->contains(t)<<"\n";
        std::cout<<"\n type = "<<typeMap->getType(t)->width_bits()<<"\n";
        cstring p4Type = parseType(typeMap->getType(t));
        unsigned width = getByteAlignment(typeMap->getType(t)->width_bits());
        // Builder.SetInsertPoint(bbInsert);
        // auto alloca;
        std::cout<<"\nwidth from getbytealignment = "<<width<<"\n";
        std::cout<<"\np4type = "<<p4Type<<"\n";
        if(t->type->toString() == "bool")    {
             Builder.CreateAlloca(Type::getInt8Ty(TheContext));    
             std::cout<<"bool\n";        
        }
        else if(p4Type == "bit") {
            Builder.CreateAlloca(ArrayType::get(Type::getInt8Ty(TheContext),width));
        }
        else if(p4Type == "int") {
            #define GETINT(SIZE) getInt##SIZETy(TheContext)
            // Builder.CreateAlloca(GETINT(width));
            std::cout<<"got int type as -- "<<GETINT(width)<<"\n";
        }

        std::cout<<"\nDeclaration_Variable\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;
        return true;
    }

    Value* EmitLLVMIR::codeGen(const IR::Declaration_Variable* t) {
        std::cout<<"\nType of decl var = "<<t->type<<"\n";
        // if(p4Type == "bool")    {
        //     return Builder.CreateAlloca(Type::getInt8Ty(TheContext),width);            
        // }
        // if(p4Type == "bit") {
        //     return Builder.CreateAlloca(ArrayType::get(Type::getInt8Ty(TheContext),width/8),width);
        // }

        std::cout<<"\nDeclaration_Variable\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";
        //return true;
        
    }

}