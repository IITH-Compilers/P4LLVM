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
#ifndef _FRONTENDS_P4_EMITLLVMIR_H_
#define _FRONTENDS_P4_EMITLLVMIR_H_

#include "ir/ir.h"
#include "ir/visitor.h"
#include "frontends/common/resolveReferences/referenceMap.h"
#include "frontends/p4/typeChecking/typeChecker.h"

#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/FileSystem.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <iterator>
#include <memory>
#include <string>
#include <vector>
#include "iostream"
#include "lib/nullstream.h"
#include "llvm/ADT/APInt.h"

using namespace llvm;

#define VERBOSE 1

#if VERBOSE
#ifndef MYDEBUG
    #define MYDEBUG(x) x
#endif
#else 
    #define MYDEBUG(x) 
#endif

#ifndef REPORTBUG
#define REPORTBUG(x) x
#endif
namespace P4 {
///////////////////////////////////////////////////
template <typename T>
class ScopeTable{
    int scope;
    std::vector<std::map<std::string, T>> dict;
    typename std::vector<std::map<std::string, T>>::iterator it;
  public:
    ScopeTable(){
        scope = 0;
        dict.push_back(std::map<std::string,T>());
    }
    void insert(std::string str, T t)   {
        dict.at(scope).insert(std::pair<std::string,T>(str,t));
        MYDEBUG(std::cout << "inserted "<<str<<" at scope - "<<scope <<std::endl;);
        typename std::map <std::string, T> map = dict.at(scope);
        // for(auto mitr = map.begin(); mitr!=map.end(); mitr++)   {
        //     MYDEBUG(std::cout<<"\n" << mitr->first << "\t-\t" << mitr->second <<"\n";)
        // }
    }
    void enterScope()   {
        scope++;
        dict.push_back(std::map<std::string,T>());        
    }
    void exitScope()    {
        if(scope>0) {
            it = dict.begin();         
            dict.erase(it+scope);
            scope--;
        }
    }
    T lookupLocal(std::string label) {
        auto entry = dict.at(scope).find(label);
        typename std::map <std::string, T> map = dict.at(scope);
        if(entry != dict.at(scope).end())
        {
            // MYDEBUG(std::cout << entry->first << std::endl; );
            return entry->second;
        }
        else
        {
            MYDEBUG(std::cout << "Not Found - " << label<< std::endl; );
            return 0;
        } 
    }
    T lookupGlobal(std::string label)   {
        for(int i=scope; i>=0; i--) {
            // MYDEBUG(std::cout<<"searching for "<<label<<" in scopetable at scope = "<<i<<std::endl;);
            auto entry = dict.at(i).find(label);
            if(entry != dict.at(i).end())   {
                // MYDEBUG(std::cout<<"returning value from scope table as -> " << entry->second << std::endl;);
                return entry->second;
            }
        }
        MYDEBUG(std::cout<<"Not found in scopetable -> " << label << std::endl;);
        printAll();
        return 0;
    }
    void printAll() {           
        for(auto vitr = dict.begin(); vitr!=dict.end();vitr++) {
            typename std::map <std::string, T> map(vitr->begin(), vitr->end());
            std::cout<<"\nmap\n--------\n";
            for(auto mitr = map.begin(); mitr!=map.end(); mitr++)   {
                std::cout<<"\n" << mitr->first << "\t-\t" << mitr->second <<"\n";
            }
            
        }
    }
    int getCurrentScope() {
        return scope;
    }
    std::map<std::string, T>& getVars(int sc) {
        std::cout << "sc = " << sc << "scope = " << scope << std::endl; 
        assert(sc>=0 && sc<=scope);
        return dict.at(sc);
    }
};

class EmitLLVMIR : public Inspector {
    Function *function;
    int i =0; //Debug info
    LLVMContext TheContext;
    std::unique_ptr<Module> TheModule;
    llvm::IRBuilder<> Builder;
    ReferenceMap*  refMap;
    TypeMap* typeMap;
    BasicBlock *bbInsert;
    raw_fd_ostream *S;
    cstring fileName;
    ScopeTable<Value*> st;
    std::map<cstring, std::vector<llvm::Value *> > action_call_args;    //append these args at end
    std::map<cstring,  std::vector<bool> > action_call_args_isout;
    //IN PROGRESS - used for GEP i.e., from member name get the field index in a struct. all the work related to getelementptr is commented
    std::map<llvm::Type *, std::map<std::string, int> > structIndexMap;

    std::map<cstring, llvm::Type *> defined_type;
    std::map<cstring, llvm::BasicBlock *> defined_state;

    std::map<cstring, std::vector<cstring> > action_list_enum;

    // Helper Functions 
    unsigned getByteAlignment(unsigned width);
    llvm::Type* getCorrespondingType(const IR::Type *t);
    llvm::Value* processExpression(const IR::Expression *e, BasicBlock* bbIf=nullptr, BasicBlock* bbElse=nullptr, bool required_alloca=false);
    void createLookUpFunction(StructType *key, StructType *value, StructType *entry, StructType *table);
    
   public:
    EmitLLVMIR(const IR::P4Program* program, cstring fileName, ReferenceMap* refMap, TypeMap* typeMap) : fileName(fileName), Builder(TheContext), refMap(refMap), typeMap(typeMap) {
        CHECK_NULL(program); 
        CHECK_NULL(fileName);
        TheModule = llvm::make_unique<Module>("p4Code", TheContext);
        std::vector<Type*> args;
        FunctionType *FT = FunctionType::get(Type::getVoidTy(TheContext), args, false);
        function = Function::Create(FT, Function::ExternalLinkage, "main", TheModule.get());
        
        bbInsert = BasicBlock::Create(TheContext, "entry", function);
        
        MYDEBUG(std::cout<< "SetInsertPoint = main\n";)
        Builder.SetInsertPoint(bbInsert);
        Builder.CreateRetVoid(); // Add this in ConstructorCall Later
        std::ostream& opStream = *openFile(fileName+".ll", true);
        setName("EmitLLVMIR");
    }

    void dumpLLVMIR() {
        std::error_code ec;         
        S = new raw_fd_ostream(fileName+".ll", ec, sys::fs::F_RW);
        assert(!TheModule->empty() && "module is empty");
        TheModule->print(*S,nullptr);
        TheModule->dump();
    }

    // Visitor function
    bool preorder(const IR::Type_Boolean* t) override;
    bool preorder(const IR::Type_Varbits* t) override;
    bool preorder(const IR::Type_Bits* t) override;
    bool preorder(const IR::Type_InfInt* t) override;
    bool preorder(const IR::Type_Dontcare* t) override;
    bool preorder(const IR::Type_Void* t) override;
    bool preorder(const IR::Type_Error* t) override;
    // bool preorder(const IR::Type_Struct* t) override;
    bool preorder(const IR::Type_Name* t) override;
    // bool preorder(const IR::Type_Header* t) override;
    // bool preorder(const IR::Type_HeaderUnion* t) override;
    bool preorder(const IR::Type_Package* t) override;
    bool preorder(const IR::Type_Control* t) override;
    bool preorder(const IR::Type_Stack* t) override;
    bool preorder(const IR::Type_Specialized* t) override;
    bool preorder(const IR::Type_Enum* t) override;
    bool preorder(const IR::Type_Typedef* t) override;
    bool preorder(const IR::Type_Unknown* t) override;
    bool preorder(const IR::Type_Tuple* t) override;
    // declarations
    bool preorder(const IR::Declaration_Constant* cst) override;
    bool preorder(const IR::Declaration_Variable* t) override;
    bool preorder(const IR::Declaration_Instance* t) override;
    bool preorder(const IR::Declaration_MatchKind* t) override;
    // expressions
    bool preorder(const IR::Constant* t) override;
    bool preorder(const IR::Slice* t) override;
    bool preorder(const IR::BoolLiteral* t) override;
    bool preorder(const IR::StringLiteral* t) override;
    bool preorder(const IR::PathExpression* t) override;
    bool preorder(const IR::Cast* t) override;
    bool preorder(const IR::Operation_Binary* t) override;
    bool preorder(const IR::Operation_Unary* t) override;
    bool preorder(const IR::ArrayIndex* t) override;
    bool preorder(const IR::TypeNameExpression* t) override;
    bool preorder(const IR::Mux* t) override;
    bool preorder(const IR::ConstructorCallExpression* t) override;
    bool preorder(const IR::Member* t) override;
    bool preorder(const IR::SelectCase* t) override;
    bool preorder(const IR::SelectExpression* t) override;
    bool preorder(const IR::ListExpression* t) override;
    bool preorder(const IR::MethodCallExpression* t) override;
    bool preorder(const IR::DefaultExpression* t) override;
    bool preorder(const IR::This* t) override;
    // vectors
    bool preorder(const IR::Vector<IR::ActionListElement>* t) override;
    bool preorder(const IR::Vector<IR::Annotation>* t) override;
    bool preorder(const IR::Vector<IR::Entry>* t) override;
    bool preorder(const IR::Vector<IR::Expression>* t) override;
    bool preorder(const IR::Vector<IR::KeyElement>* t) override;
    bool preorder(const IR::Vector<IR::Method>* t) override;
    bool preorder(const IR::Vector<IR::Node>* t) override;
    bool preorder(const IR::Vector<IR::SelectCase>* t) override;
    bool preorder(const IR::Vector<IR::SwitchCase>* t) override;
    bool preorder(const IR::IndexedVector<IR::Declaration_ID>* t) override;
    bool preorder(const IR::IndexedVector<IR::Declaration>* t) override;
    bool preorder(const IR::IndexedVector<IR::Node>* t) override;
    bool preorder(const IR::IndexedVector<IR::ParserState>* t) override;
    // // statements
    bool preorder(const IR::BlockStatement* t) override;
    bool preorder(const IR::MethodCallStatement* t) override;
    bool preorder(const IR::EmptyStatement* t) override;
    bool preorder(const IR::ReturnStatement* t) override;
    bool preorder(const IR::ExitStatement* t) override;
    bool preorder(const IR::SwitchCase* t) override;
    bool preorder(const IR::SwitchStatement* t) override;
    bool preorder(const IR::IfStatement* t) override;
    // misc
    bool preorder(const IR::Path* t) override;
    bool preorder(const IR::Annotation* t) override;
    bool preorder(const IR::P4Program* t) override;
    bool preorder(const IR::P4Action* t) override;
    bool preorder(const IR::Method* t) override;
    bool preorder(const IR::Function* t) override;
    bool preorder(const IR::ExpressionValue* t) override;
    bool preorder(const IR::ActionListElement* t) override;
    bool preorder(const IR::ActionList* t) override;
    bool preorder(const IR::Key* t) override;
    bool preorder(const IR::Property* t) override;
    bool preorder(const IR::TableProperties* t) override;
    bool preorder(const IR::EntriesList *t) override;
    bool preorder(const IR::Entry *t) override;
    bool preorder(const IR::P4Table* t) override;
    // in case it is accidentally called on a V1Program
    bool preorder(const IR::V1Program*) override;
    bool preorder(const IR::Type_Extern* t) override;
    bool preorder(const IR::Type_StructLike* t) override;
    bool preorder(const IR::AssignmentStatement* t) override;
    bool preorder(const IR::P4Control* t) override;
    // overloading in order
    bool preorder(const IR::Type_Type* t) override;
    bool preorder(const IR::P4Parser* t) override;
    bool preorder(const IR::ParserState* t) override;
    bool preorder(const IR::Type_Parser* t) override;
    bool preorder(const IR::ParameterList* t) override;
    bool preorder(const IR::Parameter* t) override;
    bool preorder(const IR::TypeParameters* t) override;
    bool preorder(const IR::Type_Var* t) override;
    bool preorder(const IR::Vector<IR::Type>* t) override;
    bool preorder(const IR::IndexedVector<IR::StatOrDecl>* t) override;
};
}  // namespace P4

#endif /* _FRONTENDS_P4_LLVMIR_H_ */