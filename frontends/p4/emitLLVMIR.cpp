/*
IIT Hyderabad

authors:
Venkata Keerthy, Pankaj K, Bhanu Prakash T, D Tharun Kumar
{cs17mtech11018, cs15btech11029, cs15btech11037, cs15mtech11002}@iith.ac.in

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

*/

#include "emitLLVMIR.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/DerivedTypes.h"

#include "methodInstance.h"


namespace P4    {
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
        MYDEBUG(std::cout<<"\nDeclaration_Variable\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        AllocaInst *alloca = Builder.CreateAlloca(getCorrespondingType(typeMap->getType(t)));
        st.insert("alloca_"+t->getName(),alloca);       
        return true;
    }

    bool EmitLLVMIR::preorder(const IR::Type_StructLike* t)
    {
        MYDEBUG(std::cout<<"\nType_StructLike\t "<<*t << "\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
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
        if(t->is<IR::Type_Header>())    {
            members.push_back(getCorrespondingType(IR::Type_Boolean::get()));
        }
        StructType *structReg = llvm::StructType::create(TheContext, members, "struct."+t->name); // 
        defined_type[t->externalName()] = structReg;

        int i=0;
        for(auto x: t->fields)
        {
            structIndexMap[structReg][std::string(x->name.name)] = i;
            i++;
        }
        structIndexMap[structReg]["valid_bit"] = i;
        
        // alloca = Builder.CreateAlloca(structReg);
        // st.insert("alloca_"+t->getName(),alloca);
        return true;
    }

    bool EmitLLVMIR::preorder(const IR::Type_Extern* t)
    {
        // typeParameters --> parameters
        // getDeclarations --> Declarations
        MYDEBUG(std::cout<<"\nType_Extern\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)   
        visit(t->typeParameters);
        preorder(&t->methods);
        return true;
    }

    bool EmitLLVMIR::preorder(const IR::ParameterList* t)
    {
        MYDEBUG(std::cout<<"\nParameterList\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        for (auto param : *t->getEnumerator())
        {
            visit(param); // visits parameter
        }
        return true;
    }

    // TODO - Pass by Reference when direction is out/inout
    bool EmitLLVMIR::preorder(const IR::Parameter* t)
    {
        MYDEBUG(std::cout<<"\nParameterX\t "<<*t << " | " << "Type = " << t->type  << "||" << typeMap->getType(t) << "\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        // MYDEBUG(std::cout << "FLAG : " <<  << std::endl;)
        visit(t->annotations);
        // AllocaInst *alloca = Builder.CreateAlloca(getCorrespondingType(t->type));
        // st.insert("alloca_"+t->getName(),alloca);
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
        MYDEBUG(std::cout<<"\nTypeParameters\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)

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
        MYDEBUG(std::cout<<"\nType_Var\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        return true;
    }


    // Completed
    bool EmitLLVMIR::preorder(const IR::P4Parser* t)
    {
        MYDEBUG(std::cout<<"\nP4Parser\t "<<*t<<"\ti = "<<i++<<" --> " << t->getName() << "\n-------------------------------------------------------------------------------------------------------------\n";)
        auto pl = t->type->getApplyParameters();
        st.enterScope();
        // if (pl->size() != 2)
        // {
        //     // this 2 is for target ebpf (bmv2 have 4)
        //     ::error("Expected parser to have exactly 2 parameters");
        //     return true;
        // }
        std::vector<Type*> parser_function_args; // need this to pass for type_parser
        for (auto p : pl->parameters)
        {
            parser_function_args.push_back(PointerType::get(getCorrespondingType(p->type), 0)); // push type of parameter
            // parser_function_args.push_back(getCorrespondingType(p->type));
        }



        FunctionType *parser_function_type = FunctionType::get(Type::getInt32Ty(TheContext), parser_function_args, false);
        Function *parser_function = Function::Create(parser_function_type, Function::ExternalLinkage,  std::string(t->getName().toString()), TheModule.get());
        Function::arg_iterator args = parser_function->arg_begin();
        function = parser_function;


        BasicBlock *init_block = BasicBlock::Create(TheContext, "entry", parser_function);
        Builder.SetInsertPoint(init_block);
        MYDEBUG(std::cout<< "SetInsertPoint = Parser Entry\n";)
        for (auto p : pl->parameters)
        {
            args->setName(std::string(p->name.name));
            // AllocaInst *alloca = Builder.CreateAlloca(args->getType());
            st.insert("alloca_"+std::string(p->name.name),args);
            // Builder.CreateStore(args, alloca);
            args++;
        }
        // TypeParameters
        //  May Not Work
        if (!t->getTypeParameters()->empty())
        {
            for (auto a : t->getTypeParameters()->parameters)
            {
                // visits Type_Var
                // a->getVarName()
                // a->getDeclId()
                // a->getP4Type()
                // return(getCorrespondingType(t->getP4Type()));
                AllocaInst *alloca = Builder.CreateAlloca(getCorrespondingType(t->getP4Type()));
                st.insert("alloca_"+a->getVarName(),alloca);
            }
        }
        // visit(t->annotations);
        // visit(t->getTypeParameters());    // visits TypeParameters

        for (auto p : t->getApplyParameters()->parameters) {
            auto type = typeMap->getType(p);
            bool initialized = (p->direction == IR::Direction::In || p->direction == IR::Direction::InOut);
            // auto value = factory->create(type, !initialized); // Create type declaration
        }
        if (t->constructorParams->size() != 0) 
            visit(t->constructorParams); //visits Vector -> ParameterList
        visit(&t->parserLocals); // visits Vector -> Declaration 
        //Declare basis block for each state
        for (auto s : t->states)  
        {
            llvm::BasicBlock* bbInsert = llvm::BasicBlock::Create(TheContext, std::string(s->name.name), parser_function);
            defined_state[s->name.name] = bbInsert;
        }
        // visit all states
        visit(&t->states);
        for (auto s : t->states)
        {
            MYDEBUG(std::cout << "Visiting State = " <<  s << std::endl;);
            visit(s);
        }
        Builder.SetInsertPoint(defined_state["accept"]); // on exit set entry of new inst in accept block
        MYDEBUG(std::cout<< "SetInsertPoint = Parser -> Accept\n";)
        st.exitScope();
        return true;
    }


    // To check for bugs after 
    bool EmitLLVMIR::preorder(const IR::ParserState* parserState)
    {
        MYDEBUG(std::cout<<"\nParserState\t "<<*parserState<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)

        if(parserState->name == "start")
        {
            // if start state then create branch
            // from main function to start block
            Builder.CreateBr(defined_state[parserState->name.name]);
        }

        // set this block as insert point
        Builder.SetInsertPoint(defined_state[parserState->name.name]);
        MYDEBUG(std::cout<< "SetInsertPoint = " << parserState->name.name;)
        if (parserState->name == "accept")
        {
            Builder.CreateRet(ConstantInt::get(Type::getInt32Ty(TheContext), 1));
            return true;

        }
        else if(parserState->name == "reject")
        {
            Builder.CreateRet(ConstantInt::get(Type::getInt32Ty(TheContext), 0));
            return true;
        }
        
        preorder(&parserState->components); // indexvector -> statordecl


        // ----------------------  Check From Here  ----------------------

        // if  select expression is null
        if (parserState->selectExpression == nullptr) // Create branch to reject(there should be some transition or select)
        {
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
        MYDEBUG(std::cout<<"\nType_Parser\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
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
        MYDEBUG(std::cout<<"\nAssignmentStatement\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        llvm::Type* llvmType = nullptr;

        if(t->left->is<IR::PathExpression>())   {
            cstring name = refMap->getDeclaration(t->left->to<IR::PathExpression>()->path)->getName();
            Value* v = st.lookupGlobal("alloca_"+name);

            // assert(v != nullptr);
            if(v==nullptr)
            {

                MYDEBUG(std::cout << "Left part of assignment not found\n";)
                return true;
            }

            llvmType = defined_type[typeMap->getType(t->left)->toString()];
            assert(llvmType != nullptr);

            Value* right = processExpression(t->right);
            
            if(right != nullptr)    {
                if(llvmType->getIntegerBitWidth() > right->getType()->getIntegerBitWidth())
                    right = Builder.CreateZExt(right, llvmType);
                //store 
                Builder.CreateStore(right,v);           
            }
            else
            {
                MYDEBUG(std::cout << "Right part of assignment not found\n";)
            }
        }

        // else {
        //  //To-Do
        // }
        
        return true;
    }

    bool EmitLLVMIR::preorder(const IR::Type_Control* t)
    {
        MYDEBUG(std::cout<<"\nType_Control\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        visit(t->annotations);
        visit(t->typeParameters);
        visit(t->applyParams);                   
        return true;
    }

    bool EmitLLVMIR::preorder(const IR::P4Control* t)
    {
        MYDEBUG(std::cout<<"\nP4Control\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        st.enterScope();
        // visit(t->constructorParams); // ParameterList
        // visit(&t->controlLocals); //IndexedVector<Declaration>
        // visit(t->body); // BlockStatement
        // return true;

        auto pl = t->type->getApplyParameters();
        // if (pl->size() != 2)
        // {
        //     // this 2 is for target ebpf (bmv2 have 4)
        //     ::error("Expected parser to have exactly 2 parameters");
        //     return true;
        // }
        std::vector<Type*> control_function_args; // need this to pass for type_parser
        for (auto p : pl->parameters)
        {
            control_function_args.push_back(PointerType::get(getCorrespondingType(p->type), 0)); // push type of parameter
            // control_function_args.push_back(getCorrespondingType(p->type));
        }



        FunctionType *control_function_type = FunctionType::get(Type::getInt32Ty(TheContext), control_function_args, false);
        Function *control_function = Function::Create(control_function_type, Function::ExternalLinkage,  std::string(t->getName().toString()), TheModule.get());
        function = control_function;
        Function::arg_iterator args = control_function->arg_begin();

        BasicBlock *init_block = BasicBlock::Create(TheContext, "entry", control_function);
        Builder.SetInsertPoint(init_block);
        for (auto p : pl->parameters)
        {
            args->setName(std::string("alloca_"+p->name.name));
            // st.insert(std::string("alloca_"+p->name.name),args);
            // AllocaInst *alloca = Builder.CreateAlloca(args->getType());
            st.insert("alloca_"+std::string(p->name.name),args);
            // Builder.CreateStore(args, alloca);
            args->dump();
            args++;
        }


        MYDEBUG(std::cout<< "SetInsertPoint = Parser Entry\n";)
        // TypeParameters
        //  May Not Work

        if (!t->getTypeParameters()->empty())
        {
            for (auto a : t->getTypeParameters()->parameters)
            {
                // visits Type_Var
                // a->getVarName()
                // a->getDeclId()
                // a->getP4Type()
                // return(getCorrespondingType(t->getP4Type()));
                AllocaInst *alloca = Builder.CreateAlloca(getCorrespondingType(t->getP4Type()));
                st.insert(std::string("alloca_"+a->getVarName()),alloca);
            }
        }
        // visit(t->annotations);
        // visit(t->getTypeParameters());    // visits TypeParameters
        MYDEBUG(std::cout << "FLAG\t" << __LINE__ << "\n";)
        for (auto p : t->getApplyParameters()->parameters) {
            auto type = typeMap->getType(p);
            bool initialized = (p->direction == IR::Direction::In || p->direction == IR::Direction::InOut);
            // auto value = factory->create(type, !initialized); // Create type declaration
        }  
        MYDEBUG(std::cout << "FLAG\t" << __LINE__ << "\n";);
     
        if (t->constructorParams->size() != 0) 
            visit(t->constructorParams); //visits Vector -> ParameterList
        visit(&t->controlLocals); // visits Vector -> Declaration 
        //Declare basis block for each state
        MYDEBUG(std::cout << "FLAG\t" << __LINE__ << "\n";);
        visit(t->body);
        st.exitScope();
        MYDEBUG(std::cout << "FLAG\t" << __LINE__<< "\n";);
        Builder.CreateRet(ConstantInt::get(Type::getInt32Ty(TheContext), 1));
        return true;
    }





    bool EmitLLVMIR::preorder(const IR::IfStatement* t) {
        MYDEBUG(std::cout<<"\nIfStatement\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        
        BasicBlock* bbIf = BasicBlock::Create(TheContext, "if.then", function);
        BasicBlock* bbElse = BasicBlock::Create(TheContext, "if.else", function);
        // if(t->condition->is<IR::MethodCallStatement>())  {MYDEBUG(std::cout<<"its true";)}
        Value* cond = processExpression(t->condition, bbIf, bbElse);
        MYDEBUG(std::cout << "processed expression for if condition\n";)
        BasicBlock* bbEnd = BasicBlock::Create(TheContext, "if.end", function);
        
        //To handle cases like if(a){//dosomething;}
        //Here 'a' is a PathExpression which would return a LoadInst on processing
        if(isa<LoadInst>(cond)) {
            MYDEBUG(std::cout << "Its a load inst\n";)
            cond->dump();
            cond->getType()->dump();
            cond = Builder.CreateICmpEQ(cond, ConstantInt::get(cond->getType(),1));
            MYDEBUG(std::cout << "created a icmp eq\n";)
        }

        Builder.CreateCondBr(cond, bbIf, bbElse);
        MYDEBUG(std::cout<< "SetInsertPoint = Ifcondition\n";)
        Builder.SetInsertPoint(bbIf);
        visit(t->ifTrue);
        Builder.CreateBr(bbEnd);

        MYDEBUG(std::cout<< "SetInsertPoint = Else Condition\n";)
        Builder.SetInsertPoint(bbElse);
        visit(t->ifFalse);
        Builder.CreateBr(bbEnd);
        
        MYDEBUG(std::cout<< "SetInsertPoint = IfEnd\n";)
        Builder.SetInsertPoint(bbEnd);
        return true;
    }





    bool EmitLLVMIR::preorder(const IR::ActionList* t)
    {
        MYDEBUG(std::cout<<"\nActionList\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        action_list_enum[function->getName().str()] = std::vector<cstring>(0);

        std::vector<Type*> members;

        AllocaInst *alloca;

        int max_size = 0;
        StructType *action_struct_corr_max = NULL;

        //enum or an integer describing the action
        members.push_back(Type::getInt32Ty(TheContext));

        for (int i=0;i<t->actionList.size();i++) {
            action_list_enum[function->getName().str()].push_back(t->actionList[i]->getName().name);

            //add union of parameters for each possible action
            //first create structures corresponding to the action parameters
            std::vector<Type*> temp_members;

            Function *f_temp = TheModule->getFunction(std::string(t->actionList[i]->getName().name));

           if (f_temp != NULL) {
                auto b = f_temp->arg_begin();
                auto e = f_temp->arg_end();
                int num_extra = action_call_args[t->actionList[i]->getName()].size();
                for (int i=0;i<num_extra;i++) {
                    e--;
                }
                while(e!=b) {
                    temp_members.push_back(b->getType());
                    b++;
                }
            }

            std::stringstream sout;
            sout << i;

            StructType *temp = llvm::StructType::create(TheContext, temp_members, llvm::StringRef("struct."+std::string(function->getName())+"anon"+sout.str()+"_t_param")); // 
            defined_type[std::string(function->getName())+"anon"+sout.str()+"_t_param"] = temp;

            /*alloca = Builder.CreateAlloca(temp);
            alloca->setName(Twine("value_anon"+sout.str()));
            st.insert("alloca_value_anon"+sout.str(),alloca);*/

            if (max_size <= TheModule->getDataLayout().getTypeAllocSize(temp)) {
                max_size = TheModule->getDataLayout().getTypeAllocSize(temp);
                action_struct_corr_max = temp;
            }
        }

        members.push_back(action_struct_corr_max);

        StructType *typeValue = llvm::StructType::create(TheContext, members, llvm::StringRef("struct."+std::string(function->getName())+"_t_value")); // 
        defined_type[std::string(function->getName())+"_t_value"] = typeValue;


        StructType *typeKey = (StructType *) defined_type[std::string(function->getName())+"_t_key"];
        
        //create type of table_entry
        members.clear();
        members.push_back(typeKey);
        members.push_back(typeValue);

        StructType *typeTableEntry = llvm::StructType::create(TheContext, members, llvm::StringRef("struct."+std::string(function->getName())+"_t_entry")); // 
        defined_type[std::string(function->getName())+"_t_entry"] = typeTableEntry;

        //create type of table
        members.clear();
        members.push_back(Type::getInt32Ty(TheContext));    //current size of table

        ArrayType* arrayType = ArrayType::get(typeTableEntry, 10);  //10 - is capacity , array of table entries
        members.push_back(arrayType);

        StructType *typeTable = llvm::StructType::create(TheContext, members, llvm::StringRef("struct."+std::string(function->getName())+"_t_table")); // 
        defined_type[std::string(function->getName())+"_t_table"] = typeTable;

        alloca = Builder.CreateAlloca(typeTable);
        alloca->setName(Twine("table"));
        st.insert("table",alloca);

        //set table size to 0
        std::vector<Value *> idx;
        idx.push_back(ConstantInt::get(TheContext, llvm::APInt(32, 0, false)));
        idx.push_back(ConstantInt::get(TheContext, llvm::APInt(32, 0, false)));
        Value *cur = Builder.CreateGEP((Value *) alloca, idx);
        Builder.CreateStore(ConstantInt::get(TheContext, llvm::APInt(32, 0, false)), cur);


        //create lookup function
        //write the table implementation dependent lookup function - right now implementation is array and lookup is bruteforce

        std::vector<Type*> args;

        args.push_back(alloca->getType());
        args.push_back(PointerType::get(typeKey,0));

        FunctionType *FT = FunctionType::get(PointerType::get(typeValue,0), args, false);

        Function *old_func = function;
        BasicBlock *old_bb = bbInsert;
        

        st.enterScope();

        function = Function::Create(FT, Function::ExternalLinkage, Twine(function->getName()+"_t_table_lookup"), TheModule.get());
        bbInsert = BasicBlock::Create(TheContext, "entry", function);
        Builder.SetInsertPoint(bbInsert);

        //set names to parameters
        auto temp_arg = function->arg_begin();
        temp_arg->setName(Twine("table")), st.insert("table",temp_arg);
        temp_arg++;
        temp_arg->setName(Twine("key")), st.insert("key",temp_arg);

        createLookUpFunction(typeKey, typeValue, typeTableEntry, typeTable);

        st.exitScope();
        //implementation of lookup ends here
        function = old_func;
        bbInsert = old_bb;
        Builder.SetInsertPoint(bbInsert);
        
        return false;
    }

    bool EmitLLVMIR::preorder(const IR::Key* t)
    {
        MYDEBUG(std::cout<<"\nKey\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        
        AllocaInst* alloca;
        std::vector<Type*> members;

        std::vector<Value *> expr;
        for(auto x: t->keyElements)
        {
            //annotations, matchType, expression
            visit(x->annotations);
            visit(x->matchType);
            expr.push_back(processExpression(x->expression));
            //comment above line and uncomment below commented code to test with dummy Key fields
            // Value* tmp = ConstantInt::get(IntegerType::get(TheContext, 64), 1024, true);
            // expr.push_back(tmp);
            
        }

        for (int i=(int)expr.size()-1;i>=0;i--) {
            members.push_back(expr[i]->getType());
        }

        StructType *structReg = llvm::StructType::create(TheContext, members, llvm::StringRef("struct."+std::string(function->getName())+"_t_key")); // 
        defined_type[std::string(function->getName())+"_t_key"] = structReg;


        for (int i=0;i<members.size();i++) {
            std::stringstream sout;
            sout << members.size()-1-i;
            structIndexMap[structReg]["field"+sout.str()] = i;
        }

        alloca = Builder.CreateAlloca(structReg);
        alloca->setName(Twine("key"));
        st.insert("key",alloca);

        for (int i=0;i<expr.size();i++) {
            std::vector<Value *> idx;
            idx.push_back(ConstantInt::get(TheContext, llvm::APInt(32, 0, false)));
            idx.push_back(ConstantInt::get(TheContext, llvm::APInt(32, (int)expr.size()-1-i, false)));
            Value *cur = Builder.CreateGEP((Value *) alloca, idx);
            Builder.CreateStore(expr[i], cur);
        }

        return false;
    }

    bool EmitLLVMIR::preorder(const IR::EntriesList *t)
    {
        MYDEBUG(std::cout<<"\nEntriesList\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        Value *temp_table = st.lookupGlobal("table");
        std::vector<Value *> idx;
        idx.push_back(ConstantInt::get(TheContext, llvm::APInt(32, 0, false)));
        idx.push_back(ConstantInt::get(TheContext, llvm::APInt(32, 0, false)));
        Value *temp_size = Builder.CreateGEP(temp_table, idx);
        for (int i=0;i<t->size();i++) {
            Value *temp1 = Builder.CreateLoad(temp_size);
            idx.resize(1), idx.push_back(ConstantInt::get(TheContext, llvm::APInt(32, 1, false)));
            Value *temp_entry = Builder.CreateGEP(temp_table, idx);
            idx.resize(1), idx.push_back(temp1);
            temp_entry = Builder.CreateGEP(temp_entry, idx);

            //for filling key in entry
            idx.resize(1), idx.push_back(ConstantInt::get(TheContext, llvm::APInt(32, 0, false)));
            Value *temp_key = Builder.CreateGEP(temp_entry, idx);
            for (int j=0;j<t->entries[i]->keys->components.size();j++) {
                Value *src = processExpression(t->entries[i]->keys->components[j]);
                idx.resize(1), idx.push_back(ConstantInt::get(TheContext, llvm::APInt(32, j, false)));
                Value *dst = Builder.CreateGEP(temp_key, idx);

                MYDEBUG(std::string type_str1;)
                MYDEBUG(std::string type_str2;)
                MYDEBUG(llvm::raw_string_ostream rso1(type_str1);)
                MYDEBUG(llvm::raw_string_ostream rso2(type_str2);)
                MYDEBUG(src->getType()->print(rso1);)
                MYDEBUG(dst->getType()->print(rso2);)
                MYDEBUG(std::cout << rso1.str() << "- lol - " << rso2.str() << std::endl;)
                Builder.CreateStore(src, dst);
            }

            //for filling value in entry
            idx.resize(1), idx.push_back(ConstantInt::get(TheContext, llvm::APInt(32, 1, false)));
            Value *temp_value = Builder.CreateGEP(temp_entry, idx);
            auto x = (IR::MethodCallExpression *) t->entries[i]->action;
            if (std::find(action_list_enum[function->getName().str()].begin(), action_list_enum[function->getName().str()].end(), ((IR::PathExpression *)(x->method))->path->name.name) != action_list_enum[function->getName().str()].end()) {
                int enum_idx = std::find(action_list_enum[function->getName().str()].begin(), action_list_enum[function->getName().str()].end(), ((IR::PathExpression *)(x->method))->path->name.name) - action_list_enum[function->getName().str()].begin();
                //store enum index
                Value *src = ConstantInt::get(TheContext, llvm::APInt(32, enum_idx, false));
                idx.resize(1), idx.push_back(ConstantInt::get(TheContext, llvm::APInt(32, 0, false)));
                Value *dst = Builder.CreateGEP(temp_value, idx);
                Builder.CreateStore(src, dst);
                //store parameters
                std::stringstream sout;
                sout << enum_idx;
                idx.resize(1), idx.push_back(ConstantInt::get(TheContext, llvm::APInt(32, 1, false)));
                dst = Builder.CreateGEP(temp_value, idx);
                StructType *params_type = (StructType *) defined_type[std::string(function->getName())+"anon"+sout.str()+"_t_param"];

                Value *tmp = Builder.CreateTruncOrBitCast(dst, PointerType::get(params_type,0)); 
                //fill each field of this struct one by one
                for (int j=0;j<x->arguments->size();j++) {
                    idx.resize(1), idx.push_back(ConstantInt::get(TheContext, llvm::APInt(32, j, false)));
                    dst = Builder.CreateGEP(dst, idx);
                    src = processExpression((*(x->arguments))[j]);
                    Builder.CreateStore(src, dst);
                }
                //done!
            }
            else {
                //unknown action
            }

            temp1 = Builder.CreateAdd(temp1, ConstantInt::get(TheContext, llvm::APInt(32, 1, false)));
            Builder.CreateStore(temp1, temp_size);
            //change size -> size + 1
        }

        //CODE FOR TABLE APPLY - change the place of this code later
        std::vector<Value*> args;
        args.push_back(st.lookupGlobal("table"));
        args.push_back(st.lookupGlobal("key"));
        /*Value *temp = Builder.CreateAlloca(PointerType::get(defined_type[std::string(function->getName())+"_t_value"]));
        temp = Builder.CreateLoad(temp);*/
        //lookup
        Value *callin = Builder.CreateCall(TheModule->getFunction(function->getName().str()+"_t_table_lookup"), args);
        Value *eq = Builder.CreateICmpEQ(callin, ConstantPointerNull::get((PointerType *) callin->getType()));

        BasicBlock *bb1 = BasicBlock::Create(TheContext, "default_action", function);
        BasicBlock *bb2 = BasicBlock::Create(TheContext, "choose_action", function);
        BasicBlock *next = BasicBlock::Create(TheContext, "sw.next", function);


        Builder.CreateCondBr(eq, bb1, bb2);
        Builder.SetInsertPoint(bb2);
        bbInsert = bb2;


        idx.resize(1), idx.push_back(ConstantInt::get(TheContext, llvm::APInt(32, 0, false))); 
        Value *temp = Builder.CreateGEP(callin, idx);
        temp = Builder.CreateLoad(temp);
        SwitchInst *sw = Builder.CreateSwitch(temp, bb1, action_list_enum[function->getName().str()].size());

        //TODO Take care of fallthrough for noaction if needed - not sure
        for (int i=0;i<action_list_enum[function->getName().str()].size();i++) {
            BasicBlock *bb = BasicBlock::Create(TheContext, Twine("action_"+action_list_enum[function->getName().str()][i]), function);
            ConstantInt *onVal = ConstantInt::get(TheContext, llvm::APInt(32, i, false));
            sw->addCase(onVal, bb);

            Builder.SetInsertPoint(bb);
            idx.resize(1), idx.push_back(ConstantInt::get(TheContext, llvm::APInt(32, 1, false))); 
            temp = Builder.CreateGEP(callin, idx);

            std::stringstream sout;
            sout << i;
            StructType *params_type = (StructType *) defined_type[function->getName().str()+"anon"+sout.str()+"_t_param"];

            temp = Builder.CreateTruncOrBitCast(temp, PointerType::get(params_type,0));

            std::vector<Value*> args;
            for (int j=0;j<params_type->getNumElements();j++) {
                idx.resize(1), idx.push_back(ConstantInt::get(TheContext, llvm::APInt(32, j, false)));
                Value* arg = Builder.CreateGEP(callin, idx);
                arg = Builder.CreateLoad(arg);
                args.push_back(arg);
            }
            for (int j=0;j<action_call_args[action_list_enum[function->getName().str()][i]].size();j++) {
                args.push_back(action_call_args[action_list_enum[function->getName().str()][i]][j]);
            }
            Value *callin2 = Builder.CreateCall(TheModule->getFunction(std::string(action_list_enum[function->getName().str()][i])), args);
            Builder.CreateBr(next);
            Builder.SetInsertPoint(bbInsert);    
        }

        //handle default_action
        Builder.SetInsertPoint(bb1);
        Builder.CreateRet(ConstantInt::get(TheContext, llvm::APInt(32, 0, false)));
        Builder.SetInsertPoint(next);
        bbInsert = next;
        return false;
    }


        void EmitLLVMIR::createLookUpFunction(StructType *key, StructType *value, StructType *entry, StructType *table) 
    {
        std::vector<Value *> idx;
        Value *temp, *temp1, *temp2, *temp3, *temp4, *temp5;
        BasicBlock *tempbb1, *tempbb2, *tempbb3;
        temp = Builder.CreateAlloca(PointerType::get(value,0));
        temp->setName("ret.addr"), st.insert("ret.addr",temp);
        temp = Builder.CreateAlloca(PointerType::get(key,0));
        temp->setName("key.addr"),st.insert("key.addr", temp);
        temp = Builder.CreateAlloca(PointerType::get(table,0));
        temp->setName("table.addr"),st.insert("table.addr", temp);
        temp = Builder.CreateAlloca(Type::getInt32Ty(TheContext));
        temp->setName("i"),st.insert("i", temp);
        Builder.CreateStore(ConstantPointerNull::get(PointerType::get(value,0)), st.lookupLocal("ret.addr"));
        Builder.CreateStore(st.lookupLocal("key"), st.lookupLocal("key.addr"));
        Builder.CreateStore(st.lookupLocal("table"), st.lookupLocal("table.addr"));
        Builder.CreateStore(ConstantInt::get(TheContext, llvm::APInt(32, 0, false)), st.lookupLocal("i"));
        bbInsert = BasicBlock::Create(TheContext, "for.cond", function);
        Builder.CreateBr(bbInsert);
        
        // for.cond
        Builder.SetInsertPoint(bbInsert);
        temp1 = Builder.CreateLoad(st.lookupLocal("i"));
        temp2 = Builder.CreateLoad(st.lookupLocal("table.addr"));

        idx.push_back(ConstantInt::get(TheContext, llvm::APInt(32, 0, false)));
        idx.push_back(ConstantInt::get(TheContext, llvm::APInt(32, 0, false)));
        temp3 = Builder.CreateGEP(temp2, idx);
        temp3 = Builder.CreateLoad(temp3);
        temp = Builder.CreateICmpSLT(temp1,temp3);
        tempbb1 = BasicBlock::Create(TheContext, "for.body", function);
        tempbb2 = BasicBlock::Create(TheContext, "for.end", function);
        Builder.CreateCondBr(temp, tempbb1, tempbb2);
        
        // for.end
        Builder.SetInsertPoint(tempbb2);
        temp = Builder.CreateLoad(st.lookupLocal("ret.addr"));
        Builder.CreateRet(temp);

        //for.body
        Builder.SetInsertPoint(tempbb1);
        temp = Builder.CreateLoad(st.lookupLocal("table.addr"));
        
        idx.clear();
        idx.push_back(ConstantInt::get(TheContext, llvm::APInt(32, 0, false)));
        idx.push_back(ConstantInt::get(TheContext, llvm::APInt(32, 1, false)));
        temp = Builder.CreateGEP(temp, idx);
        temp1 = Builder.CreateLoad(st.lookupLocal("i"));

        idx.clear();
        idx.push_back(ConstantInt::get(TheContext, llvm::APInt(32, 0, false)));
        idx.push_back(temp1);
        temp1 = Builder.CreateGEP(temp, idx);

        idx.clear();
        idx.push_back(ConstantInt::get(TheContext, llvm::APInt(32, 0, false)));
        idx.push_back(ConstantInt::get(TheContext, llvm::APInt(32, 0, false)));
        temp2 = Builder.CreateGEP(temp1, idx);
        temp3 = Builder.CreateLoad(st.lookupLocal("key.addr"));
        for (int i=0;i<key->getNumElements();i++) {
            idx.clear();
            idx.push_back(ConstantInt::get(TheContext, llvm::APInt(32, 0, false)));
            idx.push_back(ConstantInt::get(TheContext, llvm::APInt(32, i, false)));
            temp4 = Builder.CreateGEP(temp2, idx);
            temp5 = Builder.CreateGEP(temp3, idx);

            //compare them
            if (i != key->getNumElements()-1) tempbb1 = BasicBlock::Create(TheContext, "if.cont", function);
            else tempbb1 = BasicBlock::Create(TheContext, "if.then", function);
            if (i==0) tempbb2 = BasicBlock::Create(TheContext, "if.end", function);
            temp = Builder.CreateICmpEQ(temp4, temp5);
            Builder.CreateCondBr(temp, tempbb1, tempbb2);
            if (i != key->getNumElements()-1) Builder.SetInsertPoint(tempbb1);
        }

        //if.then
        Builder.SetInsertPoint(tempbb1);

        idx.clear();
        idx.push_back(ConstantInt::get(TheContext, llvm::APInt(32, 0, false)));
        idx.push_back(ConstantInt::get(TheContext, llvm::APInt(32, 1, false)));
        temp1 = Builder.CreateGEP(temp1,idx);
        Builder.CreateStore(temp1, st.lookupLocal("ret.addr"));

        Builder.CreateBr(tempbb2);

        //if.end
        Builder.SetInsertPoint(tempbb2);
        tempbb2 = BasicBlock::Create(TheContext, "for.inc", function);
        Builder.CreateBr(tempbb2);

        //for.inc
        Builder.SetInsertPoint(tempbb2);
        temp = Builder.CreateLoad(st.lookupLocal("i"));
        temp = Builder.CreateAdd(temp, ConstantInt::get(TheContext, llvm::APInt(32, 1, false)));
        Builder.CreateStore(temp, st.lookupLocal("i"));
        Builder.CreateBr(bbInsert);
    }








    Value* EmitLLVMIR::processExpression(const IR::Expression* e, BasicBlock* bbIf/*=nullptr*/, BasicBlock* bbElse/*=nullptr*/, bool required_alloca /*=false*/) {

        assert(e != nullptr);
        if(e->is<IR::Operation_Unary>())    {
            const IR::Operation_Unary* oue = e->to<IR::Operation_Unary>();
            Value* exp = processExpression(oue->expr, bbIf, bbElse);
            if(e->is<IR::Cmpl>()) 
                return Builder.CreateXor(exp,-1);

            if(e->is<IR::Neg>())    
                return Builder.CreateSub(ConstantInt::get(exp->getType(),0), exp);

            if(e->is<IR::LNot>())
                return Builder.CreateICmpEQ(exp, ConstantInt::get(exp->getType(),0));

            if (e->is<IR::Member>()) {
                MYDEBUG(std::cout << "inside member handling of processexpression\n";)
                Value *ex = processExpression(e->to<IR::Member>()->expr, nullptr, nullptr, true);
                ex->dump();
                MYDEBUG(std::cout << e->to<IR::Member>()->member.name << std::endl;)
                int ext_i = structIndexMap[((PointerType *)ex->getType())->getElementType()][std::string(e->to<IR::Member>()->member.name)];
                MYDEBUG(std::cout << "processed expression inside member, retrieved from structIndexMap as - " << ext_i << std::endl;)
                std::vector<Value *> idx;
                idx.push_back(ConstantInt::get(TheContext, llvm::APInt(32, 0, false)));
                idx.push_back(ConstantInt::get(TheContext, llvm::APInt(32, ext_i, false)));
                ex = Builder.CreateGEP(ex, idx);
                MYDEBUG(std::cout << "created GEP\n";)
                MYDEBUG(ex->dump();)
                if (required_alloca) return ex;
                else return Builder.CreateLoad(ex);
            }
        }
        if(e->is<IR::BoolLiteral>())    {
            const IR::BoolLiteral* c = e->to<IR::BoolLiteral>();            
            return ConstantInt::get(getCorrespondingType(typeMap->getType(c)),c->value);            
        }

        if(e->is<IR::Constant>()) {  
            const IR::Constant* c = e->to<IR::Constant>();
            return ConstantInt::get(getCorrespondingType(typeMap->getType(c)),(c->value).get_si());
        }


        if(e->is<IR::PathExpression>()) {
            cstring name = refMap->getDeclaration(e->to<IR::PathExpression>()->path)->getName();    
            Value* v = st.lookupGlobal("alloca_"+name);
            assert(v != nullptr);
            if(required_alloca)
            {
                return v;
            }
            else return Builder.CreateLoad(v);
        }


        if(e->is<IR::Operation_Binary>())   {
            const IR::Operation_Binary* obe = e->to<IR::Operation_Binary>();
            Value* left; Value* right;
            left = processExpression(obe->left, bbIf, bbElse);
            
            if(!(e->is<IR::LAnd>() || e->is<IR::LOr>()))    {
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

            if(e->is<IR::LAnd>())   {
                BasicBlock* bbNext = BasicBlock::Create(TheContext, "land.rhs", function);                              
                BasicBlock* bbEnd;
                
                Value* icmp1;
                Value* icmp2;

                if(obe->left->is<IR::PathExpression>()) {
                    icmp1 = Builder.CreateICmpNE(left,ConstantInt::get(left->getType(),0));
                    bbEnd = BasicBlock::Create(TheContext, "land.end", function);
                    Builder.CreateCondBr(icmp1, bbNext, bbEnd);             
                }
                else    {
                    assert(bbElse != nullptr);
                    icmp1 = left;
                    Builder.CreateCondBr(icmp1, bbNext, bbElse);
                }
                MYDEBUG(std::cout<< "SetInsertPoint = land.rhs\n";)
                Builder.SetInsertPoint(bbNext);
                BasicBlock* bbParent = bbNext->getSinglePredecessor();
                assert(bbParent != nullptr);
                
                right = processExpression(obe->right, bbIf, bbElse);
                if(obe->right->is<IR::PathExpression>())    {
                    icmp2 = Builder.CreateICmpNE(right,ConstantInt::get(right->getType(),0));   
                    Builder.CreateBr(bbEnd);
                }
                else    {
                    icmp2 = right;
                    return icmp2;       
                }
                MYDEBUG(std::cout<< "SetInsertPoint = land.end\n";)
                Builder.SetInsertPoint(bbEnd);
                PHINode* phi = Builder.CreatePHI(icmp1->getType(), 2);
                phi->addIncoming(ConstantInt::getFalse(TheContext), bbParent);
                phi->addIncoming(icmp2, bbNext);
                return phi;
            }

            if(e->is<IR::LOr>())    {
                BasicBlock* bbTrue;
                BasicBlock* bbFalse = BasicBlock::Create(TheContext, "land.rhs", function);
                
                Value* icmp1;
                Value* icmp2;
                
                if(obe->left->is<IR::PathExpression>()) {
                    bbTrue = BasicBlock::Create(TheContext, "land.end", function);
                    icmp1 = Builder.CreateICmpNE(left,ConstantInt::get(left->getType(),0));
                    Builder.CreateCondBr(icmp1, bbTrue, bbFalse);
                }
                else    {
                    assert(bbIf != nullptr);
                    icmp1 = left;
                    Builder.CreateCondBr(icmp1, bbIf, bbFalse);
                }
                MYDEBUG(std::cout<< "SetInsertPoint = land.rhs\n";)
                Builder.SetInsertPoint(bbFalse);
                BasicBlock* bbParent = bbFalse->getSinglePredecessor();
                assert(bbParent != nullptr);
                
                right = processExpression(obe->right, bbIf, bbElse);
                if(obe->right->is<IR::PathExpression>())    {               
                    icmp2 = Builder.CreateICmpNE(right,ConstantInt::get(right->getType(),0));
                    Builder.CreateBr(bbTrue);
                }
                else    {
                    icmp2 = right;
                    return icmp2;
                }
                MYDEBUG(std::cout<< "SetInsertPoint = land.end\n";)
                Builder.SetInsertPoint(bbTrue);
                PHINode* phi = Builder.CreatePHI(icmp1->getType(), 2);
                phi->addIncoming(ConstantInt::getTrue(TheContext), bbParent);
                phi->addIncoming(icmp2, bbFalse);
                return phi;
            }

            if(e->is<IR::Operation_Relation>()) {
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

        if(e->is<IR::MethodCallExpression>())    {
            MYDEBUG(std::cout<<"caught mcs\n";)
            // visit(e);
            const IR::MethodCallExpression* mce = e->to<IR::MethodCallExpression>();            
            auto mi = P4::MethodInstance::resolve(mce,refMap,typeMap);
            // auto apply = mi->to<P4::ApplyMethod>();
            // if (apply != nullptr) {
            //     processApply(apply);
            //     return false;
            // }
            // auto ef = mi->to<P4::ExternFunction>();
            // if (ef != nullptr) {
            //     processFunction(ef);
            //     return false;
            // }
            // auto ext = mi->to<P4::ExternMethod>();
            // if (ext != nullptr) {
            //     processMethod(ext);
            //     return false;
            // }
            auto bim = mi->to<P4::BuiltInMethod>();
            if (bim != nullptr) {
                if (bim->name == IR::Type_Header::isValid) {
                    // visit(bim->appliedTo);
                    //write code to return last element of struct
                    Value* alloca = processExpression(bim->appliedTo, nullptr, nullptr, true);               
                    if(isa<GetElementPtrInst>(alloca)){
                        alloca->getType()->dump();
                        // MYDEBUG(std::cout << "processed expressioninside member, retrieved from structIndexMap as - " << ext_i << std::endl;)
                        if(isa<PointerType>(alloca->getType())) {
                            auto pt = dyn_cast<PointerType>(alloca->getType());
                            unsigned i = structIndexMap[pt->getElementType()].size();
                            MYDEBUG(std::cout << "value of i = " << i << std::endl;)
                            for(auto x : structIndexMap)    {
                                x.first->dump();
                            }
                            std::vector<Value *> idx;
                            idx.push_back(ConstantInt::get(TheContext, llvm::APInt(32, 0, false)));
                            idx.push_back(ConstantInt::get(TheContext, llvm::APInt(32, i-1, false)));
                            auto ex = Builder.CreateGEP(alloca, idx);
                            ex->dump();
                            MYDEBUG(std::cout << "returning GEP from mce\n";)
                            return Builder.CreateLoad(ex);
                        }
                    }
                } 
                else if (bim->name == IR::Type_Header::setValid) {
                    visit(bim->appliedTo);
                    //set last element of struct to true
                    auto value = processExpression(bim->appliedTo, nullptr, nullptr, true);
                    return Builder.CreateStore(ConstantInt::get(Type::getInt8Ty(TheContext), 1),value);                               
                } 
                else if (bim->name == IR::Type_Header::setInvalid) {
                    visit(bim->appliedTo);
                    //set last element of struct to false
                    auto value = processExpression(bim->appliedTo, nullptr, nullptr, true);
                    return Builder.CreateStore(ConstantInt::get(Type::getInt8Ty(TheContext), 0),value);
                }
            }
        }
        ::error("Returning nullptr in processExpression");        
        return nullptr;
    }

    bool EmitLLVMIR::preorder(const IR::P4Action* t)
    {
        MYDEBUG(std::cout<<"\nP4Action\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        MYDEBUG(std::cout << "Action Name = " << t->name.name << "\n";);
        //TODO handling Annotations
        Function *old_func = function;
        BasicBlock *old_bb = Builder.GetInsertBlock();

        std::vector<Type*> args;
        std::vector<std::string> names;
        std::set<std::string> allnames;

        action_call_args_isout[t->name.name] = std::vector<bool>(false);
        int max_iter = 0;
        for (auto param : *(t->parameters)->getEnumerator())
        {
            max_iter++;
            //add this parameter type to args
            action_call_args_isout[t->name.name].push_back(param->hasOut());


            if(param->hasOut())
            {
                args.push_back(PointerType::get(getCorrespondingType(param->type), 0));
            }
            else
            {
                args.push_back(getCorrespondingType(param->type));
            }
            names.push_back("alloca_"+std::string(param->name.name));
            allnames.insert(std::string(param->name.name));

            MYDEBUG(std::cout << param->name.name << "\n";)
        }

        int actual_argsize = args.size();

        action_call_args[t->name.name] = std::vector<llvm::Value *>(0);
        //add more parameters from outer scopes


        for (int i = st.getCurrentScope(); i>=st.getCurrentScope()-1 && i>=0 ; i--) {
            auto &vars = st.getVars(i);
            for (auto vp : vars) {
                if (allnames.find(std::string(vp.first)) == allnames.end()) {
                    args.push_back(vp.second->getType());
                    action_call_args[t->name.name].push_back(vp.second);
                    names.push_back(std::string(vp.first));
                    allnames.insert(std::string(vp.first));
                    MYDEBUG(vp.second->dump();)
                }
            }
        }

        st.enterScope();

        FunctionType *FT = FunctionType::get(Type::getVoidTy(TheContext), args, false);
        
        function = Function::Create(FT, Function::ExternalLinkage, Twine(t->name.name), TheModule.get());
        bbInsert = BasicBlock::Create(TheContext, "entry", function);
        MYDEBUG(std::cout<< "SetInsertPoint = " << t->name.name << " - Entry\n";)

        Builder.SetInsertPoint(bbInsert);

        auto names_iter = names.begin();
        std::cout << __LINE__ << "\n\n";
        int iterator=0;
        for (auto arg = function->arg_begin(); arg != function->arg_end(); arg++)
        {
            //name the argument
            arg->setName(Twine(*names_iter));
            std::cout << std::string(*names_iter) << ", " ;
            if(iterator<max_iter && action_call_args_isout[t->name.name][iterator])
            {
                st.insert(std::string(*names_iter),arg);
            }
            else if(iterator<max_iter)
            {
                AllocaInst *alloca = Builder.CreateAlloca(arg->getType());
                st.insert(std::string(*names_iter),alloca);
                Builder.CreateStore(arg, alloca);
            }
            iterator++;
            arg->dump();
            names_iter++;
        }



        

        //add new parameters to scope table
        

        // Is this necessary? anyway pointers to the parameters (alloca's) will be added to scope table in IR::Parameter
        /*auto args_iter = function->arg_begin();
        for (int i=0; i< actual_argsize; i++) {
            st.insert(names[i], (Value *) args_iter);
            args_iter++;
        }*/
        
        visit(t->annotations);
        // for (auto x : *(t->parameters))
        // {
        //     if(!x->hasOut())
        //     {
        //         AllocaInst *alloca = Builder.CreateAlloca(getCorrespondingType(x->type));
        //         MYDEBUG(std::cout << __LINE__ << " Entring Parameter - " << x->getName() << "\n";)
        //         st.insert("alloca_"+x->getName(),alloca);

        //     }
        // }

        st.enterScope();
    
        visit(t->body);

        st.exitScope();

        st.exitScope();

        function = old_func;   
        bbInsert = old_bb; 
        Builder.CreateRetVoid();
        MYDEBUG(std::cout<< "SetInsertPoint = " << std::string(bbInsert->getName());)
        Builder.SetInsertPoint(bbInsert);
        return false;
    }



    bool EmitLLVMIR::preorder(const IR::MethodCallExpression* expression)
    {
        MYDEBUG(std::cout<<"\nMethodCallExpression\t "<<*expression<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
    }
    bool EmitLLVMIR::preorder(const IR::MethodCallStatement* t)
    {
        MYDEBUG(std::cout<<"\nMethodCallStatement\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        visit(t->methodCall);
        return true;
    }
    bool EmitLLVMIR::preorder(const IR::Method* t)
    {
        MYDEBUG(std::cout<<"\nMethod\t "<<t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
    //     if(t->isAbstract != 1) {
    //         return true;
    //     }
    //     std::vector<Type*> method_args; // need this to pass for type_parser
    //     llvm::Type *method_returnType = getCorrespondingType(t->type->returnType);
    //     // Templates define (t->type->getTypeParameters())
    //     for(auto p : t->type->parameters)
    //     {
    //         auto llvm_type = typeMap->getType(p);
    //         // make that type as pointer
    //         auto pointerToType = llvm::PointerType::get(llvm_type);
    //         method_args.push_back(pointerToType);
    //     }
    //     FunctionType *method_type = FunctionType::get(method_returnType, method_args, false);
    //     Function *method_function = Function::Create(method_type, Function::ExternalLinkage,  std::string(t->getName().toString()), TheModule.get());
    //     Function::arg_iterator args = method_function->arg_begin();
    //     for (auto p : t->type->parameters)
    //     {
    //         args->setName(std::string(p->name.name));
    //         args++;
        return true;
    }

      
    //     // Type_Method --> type
    //         // TypeParameters --> typeParameters
    //         // Type --> returnType
    //         // ParameterList --> parameters
    //     // isAbstract --> Bool
    //     // MYDEBUG(std::cout<< t->type << "\n"; )
    //     // MYDEBUG(std::cout<< t->type->returnType << "\n"; )
    //     MYDEBUG(std::cout<< t->type->typeParameters << "\n"; )
    //     MYDEBUG(std::cout<< t->type->parameters << "\n"; )
    //     MYDEBUG(std::cout<< t->isAbstract << "\n"; )
    //     return true;
    // }


    // if (!t->getTypeParameters()->empty())
    // {
    //     for (auto a : t->getTypeParameters()->parameters)
    //     {
    //         // visits Type_Var
    //         // a->getVarName()
    //         // a->getDeclId()
    //         // a->getP4Type()
    //         // return(getCorrespondingType(t->getP4Type()));
    //         AllocaInst *alloca = Builder.CreateAlloca(getCorrespondingType(t->getP4Type()));
    //         st.insert("alloca_"+a->getVarName(),alloca);
    //     }
    // }
    // // visit(t->annotations);
    // // visit(t->getTypeParameters());    // visits TypeParameters

    // for (auto p : t->getApplyParameters()->parameters) {
    //     auto type = typeMap->getType(p);
    //     bool initialized = (p->direction == IR::Direction::In || p->direction == IR::Direction::InOut);
    //     // auto value = factory->create(type, !initialized); // Create type declaration
    // }
    // if (t->constructorParams->size() != 0) 
    //     visit(t->constructorParams); //visits Vector -> ParameterList
    // visit(&t->parserLocals); // visits Vector -> Declaration 
    // //Declare basis block for each state












    // Helper Function
    llvm::Type* EmitLLVMIR::getCorrespondingType(const IR::Type *t)
    {
        if(!t) {
            return nullptr;
        }

        // Check if base Types
            // Boolean 
            // Integer
            //
            //
            //
        if(t->is<IR::Type_Void>()) {
            return llvm::Type::getVoidTy(TheContext);
        }
        else if(t->is<IR::Type_Error>()) {
            // Should be of enum type
            MYDEBUG(
                // std::cout << __LINE__ << std::endl;
                std::cout << "Incomplete Function : "<<__LINE__ << " : Not Yet Implemented for " << t->toString()<< std::endl;
                )
        }
        else if(t->is<IR::Type_MatchKind>()) {
            // Part of
            MYDEBUG(
                // std::cout << __LINE__ << std::endl;
                std::cout << "Incomplete Function : "<<__LINE__ << " : Not Yet Implemented for " << t->toString()<< std::endl;
                )   
        }
        else if(t->is<IR::Type_Boolean>()) {
            llvm::Type *temp = Type::getInt8Ty(TheContext);                         
            defined_type[t->toString()] = temp;
            return temp;
        }
        else if(t->is<IR::StringLiteral>()) {
            MYDEBUG(
                // std::cout << __LINE__ << std::endl;
                std::cout << "Incomplete Function : "<<__LINE__ << " : Not Yet Implemented for " << t->toString()<< std::endl;
                )  
        //     auto string_value = t->to<IR::StringLiteral>()->value;
        //     llvm::Type *temp = Type::getInt8Ty(TheContext);
        //     ArrayType *ArrayTy_0 = ArrayType::get(IntegerType::get(M.getContext(), strlen(string_value)), 4);

        //     defined_type[t->toString()] = temp;
        //     return temp;
        // }
        }
        // if int<> or bit<> /// bit<32> x; /// The type of x is Type_Bits(32); /// The type of 'bit<32>' is Type_Type(Type_Bits(32))
        else if(t->is<IR::Type_Bits>()) // Bot int <> and bit<> passes this check
        {
            const IR::Type_Bits *x =  dynamic_cast<const IR::Type_Bits *>(t);
            int width = x->width_bits();
            // To do this --> How to implement unsigned and signed int in llvm
            // Right now they both are same
            if(x->isSigned)
            {   
                llvm::Type *temp = Type::getIntNTy(TheContext, width);                
                defined_type[t->toString()] = temp;
                return temp;
            }
            else 
            {   
                llvm::Type *temp = Type::getIntNTy(TheContext, width);
                defined_type[t->toString()] = temp;
                return temp;
            }
        }
        

        // Derived Types
        else if (t->is<IR::Type_StructLike>()) 
        {
            if (!t->is<IR::Type_Struct>() && !t->is<IR::Type_Header>() && !t->is<IR::Type_HeaderUnion>())
            {
                MYDEBUG(std::cout<<__LINE__ <<"Error: Unknown Type : " << t->toString() << std::endl);
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
            // auto canon = typeMap->getType(t);
            defined_type[t->toString()] = getCorrespondingType(canon);
            return (defined_type[t->toString()]);
        }
        else if(t->is<IR::Type_Tuple>())
        {
            MYDEBUG(
                // std::cout << __LINE__ << std::endl;
                std::cout << "Incomplete Function : "<<__LINE__ << " : Not Yet Implemented for " << t->toString()<< std::endl;
                )   
        }

        else if(t->is<IR::Type_Set>())
        {
            MYDEBUG(
                // std::cout << __LINE__ << std::endl;
                std::cout << "Incomplete Function : "<<__LINE__ << " : Not Yet Implemented for " << t->toString()<< std::endl;
                )
        }
        else if(t->is<IR::Type_Stack>())
        {
            MYDEBUG(
                // std::cout << __LINE__ << std::endl;
                std::cout << "Incomplete Function : "<<__LINE__ << " : Not Yet Implemented for " << t->toString()<< std::endl;
                )
        }
        else if(t->is<IR::Type_String>())
        {
            MYDEBUG(
                // std::cout << __LINE__ << std::endl;
                std::cout << "Incomplete Function : "<<__LINE__ << " : Not Yet Implemented for " << t->toString()<< std::endl;
                )
        }

        else if(t->is<IR::Type_Table>())
        {
            MYDEBUG(
                // std::cout << __LINE__ << std::endl;  
                std::cout << "Incomplete Function : "<<__LINE__ << " : Not Yet Implemented for " << t->toString()<< std::endl;
                )
        }
        else if(t->is<IR::Type_Varbits>())
        {
            MYDEBUG(
                // std::cout << __LINE__ << std::endl;
                std::cout << "Incomplete Function : "<<__LINE__ << " : Not Yet Implemented for " << t->toString()<< std::endl;
                )
        }
        else if(t->is<IR::Type_Var>())
        {
            MYDEBUG(
                // std::cout << __LINE__ << std::endl;      
            // t->getVarName()
            // t->getP4Type()

                std::cout << "Incomplete Function : "<<__LINE__ << " : Not Yet Implemented for " << t->toString() << std::endl;
                )
            // return(getCorrespondingType(t->getP4Type()));
        }
        else if(t->is<IR::Type_Enum>())
        {
            MYDEBUG(
                // std::cout << __LINE__ << std::endl;
                const IR::Type_Enum *t_enum = dynamic_cast<const IR::Type_Enum *>(t);

                for(auto x: t_enum->members)
                {
                    // std::cout << __LINE__ << std::endl;
                    std::cout << x->name << " " << x->declid << std::endl;
                }
                std::cout << "Incomplete Function : " <<__LINE__ << " : Not Yet Implemented for Type_Enum " << t->toString() << std::endl;
            )
        }
        else
        {
            MYDEBUG(
                // std::cout << __LINE__ << std::endl;
                std::cout << "Incomplete Function : "<<__LINE__ << " : Not Yet Implemented for " << t->toString() << "==> getType ==> " << typeMap->getType(t) << std::endl;
                )
        }
        MYDEBUG(std::cout << "Returning Int32 pointer for Incomplete Type" << std::endl;)
        return(Type::getInt32PtrTy(TheContext));
    }
    #define VECTOR_VISIT(V, T, SS)                                                      \
        bool EmitLLVMIR::preorder(const IR:: V <IR::T> *v){                             \
        MYDEBUG(std::cout<<"\n" << SS << "\t "<<*v<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)\
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
        MYDEBUG(std::cout<<"\nType_Boolean\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        return true;
    }

    bool EmitLLVMIR::preorder(const IR::Type_Varbits* t)
    {
        MYDEBUG(std::cout<<"\nType_Varbits\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        return true;
    }

    bool EmitLLVMIR::preorder(const IR::Type_Bits* t)
    {
        MYDEBUG(std::cout<<"\nType_Bits\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        return true;
    }

    bool EmitLLVMIR::preorder(const IR::Type_InfInt* t)
    {
        MYDEBUG(std::cout<<"\nType_InfInt\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        return true;
    }

    bool EmitLLVMIR::preorder(const IR::Type_Dontcare* t)
    {
        MYDEBUG(std::cout<<"\nType_Dontcare\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        return true;
    }

    bool EmitLLVMIR::preorder(const IR::Type_Void* t)
    {
        MYDEBUG(std::cout<<"\nType_Void\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        return true;
    }

    bool EmitLLVMIR::preorder(const IR::Type_Error* t)
    {
        MYDEBUG(std::cout<<"\nType_Error\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        return true;
    }

    bool EmitLLVMIR::preorder(const IR::Type_Name* t)
    {
        MYDEBUG(std::cout<<"\nType_Name\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        return true;

    }

    bool EmitLLVMIR::preorder(const IR::Type_Package* t)
    {
        MYDEBUG(std::cout<<"\nType_Package\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        return true;
    }

    bool EmitLLVMIR::preorder(const IR::Type_Stack* t)
    {
        MYDEBUG(std::cout<<"\nType_Stack\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        return true;
    }

    bool EmitLLVMIR::preorder(const IR::Type_Specialized* t)
    {
        MYDEBUG(std::cout<<"\nType_Specialized\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        return true;
    }

    bool EmitLLVMIR::preorder(const IR::Type_Enum* t)
    {
        MYDEBUG(std::cout<<"\nType_Enum\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        return true;
    }

    bool EmitLLVMIR::preorder(const IR::Type_Typedef* t)
    {
        MYDEBUG(std::cout<<"\nType_Typedef\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        return true;
    }

    bool EmitLLVMIR::preorder(const IR::Type_Type* t)
    {
        MYDEBUG(std::cout<<"\nType_Type\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        return true;
    }
    
    bool EmitLLVMIR::preorder(const IR::Type_Unknown* t)
    {
        MYDEBUG(std::cout<<"\nType_Unknown\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        return true;
    }

    bool EmitLLVMIR::preorder(const IR::Type_Tuple* t)
    {
        MYDEBUG(std::cout<<"\nType_Tuple\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        return true;
    }

    // declarations
    bool EmitLLVMIR::preorder(const IR::Declaration_Constant* cst)
    {
        MYDEBUG(std::cout<<"\nDeclaration_Constant\t "<<*cst<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        return true;
    }

    bool EmitLLVMIR::preorder(const IR::Declaration_Instance* t)
    {
        MYDEBUG(std::cout<<"\nDeclaration_Instance\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        return true;
    }

    bool EmitLLVMIR::preorder(const IR::Declaration_MatchKind* t)
    {
        MYDEBUG(std::cout<<"\nDeclaration_MatchKind\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        return true;
    }

    // expressions
    bool EmitLLVMIR::preorder(const IR::Constant* t)
    {
        MYDEBUG(std::cout<<"\nConstant\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        return true;
    }

    bool EmitLLVMIR::preorder(const IR::Slice* t)
    {
        MYDEBUG(std::cout<<"\nSlice\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        return true;
    }

    bool EmitLLVMIR::preorder(const IR::BoolLiteral* t)
    {
        MYDEBUG(std::cout<<"\nBoolLiteral\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        return true;
    }

    bool EmitLLVMIR::preorder(const IR::StringLiteral* t)
    {
        MYDEBUG(std::cout<<"\nStringLiteral\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        return true;
    }

    bool EmitLLVMIR::preorder(const IR::PathExpression* t)
    {
        MYDEBUG(std::cout<<"\nPathExpression\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        return true;
    }

    bool EmitLLVMIR::preorder(const IR::Cast* t)
    {
        MYDEBUG(std::cout<<"\nCast\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        return true;
    }

    bool EmitLLVMIR::preorder(const IR::Operation_Binary* t)
    {
        MYDEBUG(std::cout<<"\nOperation_Binary\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        return true;
    }

    bool EmitLLVMIR::preorder(const IR::Operation_Unary* t)
    {
        MYDEBUG(std::cout<<"\nOperation_Unary\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        return true;
    }

    bool EmitLLVMIR::preorder(const IR::ArrayIndex* t)
    {
        MYDEBUG(std::cout<<"\nArrayIndex\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        return true;
    }

    bool EmitLLVMIR::preorder(const IR::TypeNameExpression* t)
    {
        MYDEBUG(std::cout<<"\nTypeNameExpression\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        return true;
    }

    bool EmitLLVMIR::preorder(const IR::Mux* t)
    {
        MYDEBUG(std::cout<<"\nMux\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        return true;
    }

    bool EmitLLVMIR::preorder(const IR::ConstructorCallExpression* t)
    {
        MYDEBUG(std::cout<<"\nConstructorCallExpression\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        return true;
    }

    bool EmitLLVMIR::preorder(const IR::Member* t)
    {
        MYDEBUG(std::cout<<"\nMember\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        return true;
    }

    bool EmitLLVMIR::preorder(const IR::SelectCase* t)
    {
        //handled inside SelectExpression
        MYDEBUG(std::cout<<"\nSelectCase\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        return true;
    }

    bool EmitLLVMIR::preorder(const IR::SelectExpression* t)
    {
        MYDEBUG(std::cout<<"\nSelectExpression\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        
        //right now, only supports single key based select. and cases are allowed only on integer constant expressions
        /*
        if (defined_state.find("start") == defined_state.end()) {
            defined_state["start"] = BasicBlock::Create(TheContext, "start", function);
        }
        */
        if (defined_state.find("reject") == defined_state.end()) {
            defined_state["reject"] = BasicBlock::Create(TheContext, "reject", function);
        }

        SwitchInst *sw = Builder.CreateSwitch(processExpression(t->select->components[0],NULL,NULL), defined_state["reject"], t->selectCases.size());
        //comment above line and uncomment below commented code to test selectexpression with dummy select key
        
        // Value* tmp = ConstantInt::get(IntegerType::get(TheContext, 64), 1024, true);
        // SwitchInst *sw = Builder.CreateSwitch(tmp, defined_state["reject"], t->selectCases.size());
        

        bool issetdefault = false;
        for (unsigned i=0;i<t->selectCases.size();i++) {
            //visit(t->selectCases[i]);
            if (dynamic_cast<const IR::DefaultExpression *>(t->selectCases[i]->keyset)) {
                if (issetdefault) continue;
                else {
                    issetdefault = true;
                    if (defined_state.find(t->selectCases[i]->state->path->asString()) == defined_state.end()) {
                        defined_state[t->selectCases[i]->state->path->asString()] = BasicBlock::Create(TheContext, Twine(t->selectCases[i]->state->path->asString()), function);
                    }
                    sw->setDefaultDest(defined_state[t->selectCases[i]->state->path->asString()]);
                }
            }
            else if (dynamic_cast<const IR::Constant *>(t->selectCases[i]->keyset)) {
                if (defined_state.find(t->selectCases[i]->state->path->asString()) == defined_state.end()) {
                    defined_state[t->selectCases[i]->state->path->asString()] = BasicBlock::Create(TheContext, Twine(t->selectCases[i]->state->path->asString()), function);
                }
                ConstantInt *onVal = (ConstantInt *) processExpression(t->selectCases[i]->keyset);

                sw->addCase(onVal, defined_state[t->selectCases[i]->state->path->asString()]);
            }
        }
        return false;
    }

    bool EmitLLVMIR::preorder(const IR::ListExpression* t)
    {
        MYDEBUG(std::cout<<"\nListExpression\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        return true;
    }


    bool EmitLLVMIR::preorder(const IR::DefaultExpression* t)
    {
        MYDEBUG(std::cout<<"\nDefaultExpression\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        return true;
    }

    bool EmitLLVMIR::preorder(const IR::This* t)
    {
        MYDEBUG(std::cout<<"\nThis\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        return true;
    }

    // // statements
    bool EmitLLVMIR::preorder(const IR::BlockStatement* t)
    {
        MYDEBUG(std::cout<<"\nBlockStatement\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        return true;
    }



    bool EmitLLVMIR::preorder(const IR::EmptyStatement* t)
    {
        MYDEBUG(std::cout<<"\nEmptyStatement\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        return true;
    }
    
    bool EmitLLVMIR::preorder(const IR::ReturnStatement* t)
    {
        MYDEBUG(std::cout<<"\nReturnStatement\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        return true;
    }

    bool EmitLLVMIR::preorder(const IR::ExitStatement* t)
    {
        MYDEBUG(std::cout<<"\nExitStatement\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        return true;
    }

    bool EmitLLVMIR::preorder(const IR::SwitchCase* t)
    {
        MYDEBUG(std::cout<<"\nSwitchCase\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        return true;
    }

    bool EmitLLVMIR::preorder(const IR::SwitchStatement* t)
    {
        MYDEBUG(std::cout<<"\nSwitchStatement\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        return true;
    }

    // misc
    bool EmitLLVMIR::preorder(const IR::Path* t)
    {
        MYDEBUG(std::cout<<"\nPath\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        return true;
    }

    bool EmitLLVMIR::preorder(const IR::Annotation* t)
    {
        MYDEBUG(std::cout<<"\nAnnotation\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        return true;
    }

    bool EmitLLVMIR::preorder(const IR::P4Program* t)
    {
        MYDEBUG(std::cout<<"\nP4Program\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        return true;
    }

   

    bool EmitLLVMIR::preorder(const IR::Function* t)
    {
        MYDEBUG(std::cout<<"\nFunction\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        return true;
    }

    bool EmitLLVMIR::preorder(const IR::ExpressionValue* t)
    {
        MYDEBUG(std::cout<<"\nExpressionValue\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        return true;
    }

    bool EmitLLVMIR::preorder(const IR::ActionListElement* t)
    {
        MYDEBUG(std::cout<<"\nActionListElement\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        return true;
    }



    bool EmitLLVMIR::preorder(const IR::Property* t)
    {
        MYDEBUG(std::cout<<"\nProperty\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        return true;
    }

    bool EmitLLVMIR::preorder(const IR::TableProperties* t)
    {
        MYDEBUG(std::cout<<"\nTableProperties\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        return true;
    }

    bool EmitLLVMIR::preorder(const IR::Entry *t)
    {
        MYDEBUG(std::cout<<"\nEntry\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        return true;
    }

    bool EmitLLVMIR::preorder(const IR::P4Table* t)
    {
        MYDEBUG(std::cout<<"\nP4Table\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        return true;
    }

    // in case it is accidentally called on a V1Program
    bool EmitLLVMIR::preorder(const IR::V1Program* t)
    {
        MYDEBUG(std::cout<<"\nV1Program\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";)
        return true;
        
    }
}
