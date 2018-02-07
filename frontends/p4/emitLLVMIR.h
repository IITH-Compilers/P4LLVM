
#ifndef _FRONTENDS_P4_EMITLLVMIR_H_
#define _FRONTENDS_P4_EMITLLVMIR_H_

#include "ir/ir.h"
#include "ir/visitor.h"
#include "frontends/common/resolveReferences/referenceMap.h"
#include "frontends/p4/typeChecking/typeChecker.h"
// #include "ir/node.h"
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
// #include "llvm/IR/Verifier.h"
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

// class Value;
using namespace llvm;
namespace P4 {

class EmitLLVMIR : public Inspector {
    // map<cstring,cstring> llvmTypeMap;
    // llvmTypeMap.insert(pair<cstring,cstring> ("bool","i8"));
    // llvmTypeMap.insert(pair<cstring,cstring> ("bit","i1"));
    // llvmTypeMap.insert(pair<cstring,cstring> ("","i1"));

    std::unique_ptr<Function> function;
    int i =0;
    LLVMContext TheContext;
    std::unique_ptr<Module> TheModule;
    llvm::IRBuilder<> Builder;
    ReferenceMap*  refMap;
    TypeMap* typeMap;
    BasicBlock *bbInsert;
    raw_fd_ostream *S;
    cstring fileName;
   public:
    EmitLLVMIR(const IR::P4Program* program, cstring fileName, ReferenceMap* refMap, TypeMap* typeMap) : fileName(fileName), Builder(TheContext), refMap(refMap), typeMap(typeMap) {
        CHECK_NULL(program); 
        CHECK_NULL(fileName);
        std::cout<< this->typeMap << "\t" << typeMap << "\n";
        TheModule = llvm::make_unique<Module>("p4Code", TheContext);
        std::vector<Type*> args;
        FunctionType *FT = FunctionType::get(Type::getVoidTy(TheContext), args, false);
        Function *function = Function::Create(FT, Function::ExternalLinkage, "ebpf_filter", TheModule.get());
        bbInsert = BasicBlock::Create(TheContext, "entry", function);
        Builder.SetInsertPoint(bbInsert);
        // bbInsert = BB;
        std::ostream& opStream = *openFile(fileName+".ll", true);
        setName("EmitLLVMIR");
        
        // function->print(S,nullptr);

    }
    ~EmitLLVMIR () {
        std::error_code ec;         
        S = new raw_fd_ostream(fileName+".ll", ec, sys::fs::F_RW);
        TheModule->print(*S,nullptr);
        std::cout<<"\n************************************************************************\n";
    }
    unsigned getByteAlignment(unsigned width);
    cstring parseType(const IR::Type*);
    Value *codeGen(const IR::Declaration_Variable *t);


    // bool preorder(const IR::Type_Boolean* t)  {std::cout<<"\nType_Boolean\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;}
    // bool preorder(const IR::Type_Varbits* t) {std::cout<<"\nType_Varbits\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};
    // bool preorder(const IR::Type_Bits* t) {std::cout<<"\nType_Bits\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};
    // bool preorder(const IR::Type_InfInt* t) {std::cout<<"\nType_InfInt\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};
    // bool preorder(const IR::Type_Var* t) {std::cout<<"\nType_Var\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};
    // bool preorder(const IR::Type_Dontcare* t) {std::cout<<"\nType_Dontcare\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};
    // bool preorder(const IR::Type_Void* t) {std::cout<<"\nype_Void\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};
    // bool preorder(const IR::Type_Error* t) {std::cout<<"\nType_Error\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};
    // bool preorder(const IR::Type_Struct* t) {std::cout<<"\nType_Struct\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;}
    // bool preorder(const IR::Type_Header* t) {std::cout<<"\nType_Header\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;}
    // bool preorder(const IR::Type_HeaderUnion* t) {std::cout<<"\nType_HeaderUnion\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;}
    // bool preorder(const IR::Type_Package* t) {std::cout<<"\nType_Package\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};
    // bool preorder(const IR::Type_Parser* t) {std::cout<<"\nType_Parser\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};
    // bool preorder(const IR::Type_Control* t) {std::cout<<"\nype_Control\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};
    // bool preorder(const IR::Type_Name* t) {std::cout<<"\nType_Name\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};
    // bool preorder(const IR::Type_Stack* t) {std::cout<<"\nType_Stack\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};
    // bool preorder(const IR::Type_Specialized* t) {std::cout<<"\nType_Specialized\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};
    // bool preorder(const IR::Type_Enum* t) {std::cout<<"\nType_Enum\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};
    // bool preorder(const IR::Type_Typedef* t) {std::cout<<"\nType_Typedef\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};
    // bool preorder(const IR::Type_Extern* t) {std::cout<<"\nType_Extern\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};
    // bool preorder(const IR::Type_Unknown* t) {std::cout<<"\nType_Unknown\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};
    // bool preorder(const IR::Type_Tuple* t) {std::cout<<"\nType_Tuple\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};

    // // declarations
    // bool preorder(const IR::Declaration_Constant* cst) {std::cout<<"\nDeclaration_Constant\t "<<*cst<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};
    bool preorder(const IR::Declaration_Variable* t)override; 
    // bool preorder(const IR::Declaration_Instance* t) {std::cout<<"\nDeclaration_Instance\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};
    // bool preorder(const IR::Declaration_MatchKind* t) {std::cout<<"\nDeclaration_MatchKind\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};

    // // expressions
    // bool preorder(const IR::Constant* t) {std::cout<<"\nConstant\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};;
    // bool preorder(const IR::Slice* t) {std::cout<<"\nSlice\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};;
    // bool preorder(const IR::BoolLiteral* t) {std::cout<<"\nBoolLiteral\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};
    // bool preorder(const IR::StringLiteral* t) {std::cout<<"\nStringLiteral\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};;
    // bool preorder(const IR::PathExpression* t) {std::cout<<"\nPathExpression\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};;
    // bool preorder(const IR::Cast* t) {std::cout<<"\nCast\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};;
    // bool preorder(const IR::Operation_Binary* t) {std::cout<<"\nOperation_Binary\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};
    // bool preorder(const IR::Operation_Unary* t) {std::cout<<"\nOperation_Unary\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};
    // bool preorder(const IR::ArrayIndex* t) {std::cout<<"\nArrayIndex\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};;
    // bool preorder(const IR::TypeNameExpression* t) {std::cout<<"\nTypeNameExpression\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};
    // bool preorder(const IR::Mux* t) {std::cout<<"\nMux\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};;
    // bool preorder(const IR::ConstructorCallExpression* t) {std::cout<<"\nConstructorCallExpression\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};;
    // bool preorder(const IR::Member* t) {std::cout<<"\nMember\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};;
    // bool preorder(const IR::SelectCase* t) {std::cout<<"\nSelectCase\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};;
    // bool preorder(const IR::SelectExpression* t) {std::cout<<"\nSelectExpression\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};;
    // bool preorder(const IR::ListExpression* t) {std::cout<<"\nListExpression\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};;
    // bool preorder(const IR::MethodCallExpression* t) {std::cout<<"\nMethodCallExpression\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};;
    // bool preorder(const IR::DefaultExpression* t) {std::cout<<"\nDefaultExpression\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};;
    // bool preorder(const IR::This* t) {std::cout<<"\nThis\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};;

    // // vectors
    // bool preorder(const IR::Vector<IR::ActionListElement>* t) {std::cout<<"\nActionListElement\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};;
    // bool preorder(const IR::Vector<IR::Annotation>* t) {std::cout<<"\nAnnotation\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};;
    // bool preorder(const IR::Vector<IR::Entry>* t) {std::cout<<"\nEntry\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};;
    // bool preorder(const IR::Vector<IR::Expression>* t) {std::cout<<"\nExpression\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};
    // bool preorder(const IR::Vector<IR::KeyElement>* t) {std::cout<<"\nKeyElement\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};;
    // bool preorder(const IR::Vector<IR::Method>* t) {std::cout<<"\nMethod\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};
    // bool preorder(const IR::Vector<IR::Node>* t) {std::cout<<"\nNode\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};
    // bool preorder(const IR::Vector<IR::SelectCase>* t) {std::cout<<"\nSelectCase\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};;
    // bool preorder(const IR::Vector<IR::SwitchCase>* t) {std::cout<<"\nSwitchCase\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};;
    // bool preorder(const IR::Vector<IR::Type>* t) {std::cout<<"\ntype\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};
    // bool preorder(const IR::IndexedVector<IR::Declaration_ID>* t) {std::cout<<"\n Declaration_ID\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};;
    // bool preorder(const IR::IndexedVector<IR::Declaration>* t) {std::cout<<"\nDeclaration\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};;
    // bool preorder(const IR::IndexedVector<IR::Node>* t) {std::cout<<"\nNode\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};;
    // bool preorder(const IR::IndexedVector<IR::ParserState>* t) {std::cout<<"\nParserState\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};;
    // bool preorder(const IR::IndexedVector<IR::StatOrDecl>* t) {std::cout<<"\nStatOrDecl\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};;

    // // statements
    // bool preorder(const IR::AssignmentStatement* t) override;//{std::cout<<"\nAssignmentStatement\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};
    // bool preorder(const IR::BlockStatement* t) {std::cout<<"\nBlockStatement\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};;
    // bool preorder(const IR::MethodCallStatement* t) {std::cout<<"\nMethodCallStatement\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};
    // bool preorder(const IR::EmptyStatement* t) {std::cout<<"\nEmptyStatement\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};;
    // bool preorder(const IR::ReturnStatement* t) {std::cout<<"\nReturnStatement\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};;
    // bool preorder(const IR::ExitStatement* t) {std::cout<<"\nExitStatement\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};;
    // bool preorder(const IR::SwitchCase* t) {std::cout<<"\nSwitchCase\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};;
    // bool preorder(const IR::SwitchStatement* t) {std::cout<<"\nSwitchStatement\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};;
    // bool preorder(const IR::IfStatement* t) {std::cout<<"\nIfStatement\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};;

    // // misc
    // bool preorder(const IR::Path* t) {std::cout<<"\nPath\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};;
    // bool preorder(const IR::Parameter* t) {std::cout<<"\nParameter\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};;
    // bool preorder(const IR::Annotation* t) {std::cout<<"\nAnnotation\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};;
    // bool preorder(const IR::P4Program* t) {std::cout<<"\nP4Program\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};
    // bool preorder(const IR::P4Control* t) {std::cout<<"\nP4Control\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};
    // bool preorder(const IR::P4Action* t) {std::cout<<"\nP4Action\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};
    // bool preorder(const IR::ParserState* t) {std::cout<<"\nParserState\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};
    // bool preorder(const IR::P4Parser* t) {std::cout<<"\nP4Parser\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};
    // bool preorder(const IR::TypeParameters* t) {std::cout<<"\nTypeParameters\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};;
    // bool preorder(const IR::ParameterList* t) {std::cout<<"\nParameterList\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};;
    // bool preorder(const IR::Method* t) {std::cout<<"\nMethod\t "<<t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};
    // bool preorder(const IR::Function* t) {std::cout<<"\nFunction\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};

    // bool preorder(const IR::ExpressionValue* t) {std::cout<<"\nExpressionValue\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};;
    // bool preorder(const IR::ActionListElement* t) {std::cout<<"\nActionListElement\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};;
    // bool preorder(const IR::ActionList* t) {std::cout<<"\nActionList\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};;
    // bool preorder(const IR::Key* t) {std::cout<<"\nKey\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};;
    // bool preorder(const IR::Property* t) {std::cout<<"\nProperty\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};;
    // bool preorder(const IR::TableProperties* t) {std::cout<<"\nTableProperties\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};;
    // bool preorder(const IR::EntriesList *t) {std::cout<<"\nEntriesList\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};;
    // bool preorder(const IR::Entry *t) {std::cout<<"\nEntry\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};;
    // bool preorder(const IR::P4Table* t) {std::cout<<"\nP4Table\t "<<*t<<"\ti = "<<i++<<"\n-------------------------------------------------------------------------------------------------------------\n";return true;};;

    // // in case it is accidentally called on a V1Program
    // bool preorder(const IR::V1Program*) {return false;}
};


}  // namespace P4

#endif /* _FRONTENDS_P4_LLVMIR_H_ */
