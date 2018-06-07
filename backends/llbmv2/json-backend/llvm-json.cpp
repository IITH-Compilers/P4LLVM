#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/IR/Instructions.h"
#include <vector>
#include "../JsonObjects.h"
// #include "../helpers.h"
#include "lib/json.h"
#include "emitHeader.h"
#include "emitParser.h"
#include "emitDeparser.h"
#include <fstream>
#include <iostream>
#include "helpers.h"

using namespace llvm;

namespace
{
struct JsonBackend : public ModulePass
{
public:
    static char ID;
	JsonBackend() : ModulePass(ID) {
		json = new LLBMV2::JsonObjects();
		pc = new LLBMV2::ParserConverter(json);
		cd = new LLBMV2::ConvertDeparser(json);
	}

	~JsonBackend() {
		delete json;
		delete pc;
		delete cd;
	}

	virtual bool runOnModule(Module &M) {
		// initialize json objects
		populateJsonObjects(M);
		populateStruct2Type(M.getIdentifiedStructTypes(),
							M.getNamedMetadata("header"),
							M.getNamedMetadata("struct"),
							M.getNamedMetadata("header_union"));
		emitLocalVariables(M);
		// Iterate on functions
		for (auto fun = M.begin(); fun != M.end(); fun++) {
			errs() << "Name of the function is: " << (&*fun)->getName() << "\n";
			emitHeaders(&*fun);
		}
		emitParser(M);
		emitDeparser(M);
		printJsonToFile(M.getSourceFileName()+".ll.json");
		return false;
	}
	bool emitHeaders(Function *F);
	void emitLocalVariables(Module &M);
	void emitParser(Module &M);
	void emitDeparser(Module &M);

private:
		Util::JsonObject jsonTop;
		LLBMV2::JsonObjects* json;
		Util::JsonArray *counters;
		Util::JsonArray *externs;
		Util::JsonArray *field_lists;
		Util::JsonArray *learn_lists;
		Util::JsonArray *meter_arrays;
		Util::JsonArray *register_arrays;
		Util::JsonArray *force_arith;
		Util::JsonArray *field_aliases;
		void populateJsonObjects(Module &M);
		void populateStruct2Type(std::vector<StructType *> structs,
								NamedMDNode *header_md,
								NamedMDNode *struct_md,
								NamedMDNode *header_union_md);
		LLBMV2::ConvertHeaders ch;
		LLBMV2::ParserConverter *pc;
		LLBMV2::ConvertDeparser *cd;
		void printJsonToFile(const std::string fn);
		std::map<llvm::StructType*, std::string> *struct2Type;
};
}

char JsonBackend::ID = 0;

void JsonBackend::emitDeparser(Module &M) {
	for (auto fn = M.begin(); fn != M.end(); fn++) {
		if ((&*fn)->getAttributes().getFnAttributes().hasAttribute("Deparser")) {
			cd->processDeparser((&*fn));
		}
	}
}

void JsonBackend::emitParser(Module &M) {
	for (auto fn = M.begin(); fn != M.end(); fn++) {
		if ((&*fn)->getAttributes().getFnAttributes().hasAttribute("parser")) {
			pc->processParser((&*fn));
		}
	}
}

void JsonBackend::emitLocalVariables(Module &M) {
	// Collect local variable.
	// A local varible is the instance that is not from parameters list
	// Finds all the allocas that are not of function paramerter
	// FIXME: To differentiate local allocas from param allocas
	// 		This code checks for struct names, if there is no name it is local
	SmallVector<AllocaInst*, 8> *allocaList = new SmallVector<AllocaInst*, 8>();
	for(auto fn = M.begin(); fn != M.end(); fn++) {
		for(auto inst = inst_begin(&*fn); inst != inst_end(&*fn); inst++) {
			if(auto alloc = dyn_cast<AllocaInst>(&*inst)) {
				if(alloc->getAllocatedType()->isStructTy() &&
				   dyn_cast<StructType>(alloc->getAllocatedType())->getName() == "") {
					allocaList->push_back(alloc);
				}
				else if(!alloc->getAllocatedType()->isStructTy()) {
					// All non-stuct types are local, as params won't contian non-stuct type
					allocaList->push_back(alloc);
				}
			}
		}
	}
	ch.processHeaders(allocaList, struct2Type, json);
}

void JsonBackend::populateStruct2Type(std::vector<StructType *> structs,
													NamedMDNode *header_md,
													NamedMDNode *struct_md,
													NamedMDNode *header_union_md) {

	struct2Type = new std::map<llvm::StructType*, std::string>();
	for (auto st : structs) {
		bool found = false;
		for (auto op = 0u; op != header_md->getOperand(0)->getNumOperands(); op++) {
			MDString *mdstr = dyn_cast<MDString>(header_md->getOperand(0)->getOperand(op));
			assert(mdstr != nullptr);
			if (st->getName().equals(mdstr->getString())) {
				errs() << st->getName() << " is of Header type\n";
				(*struct2Type)[st] = "header";
				found = true;
				break;
			}
		}
		if(found)
			continue;
		for (auto op = 0u; op != struct_md->getOperand(0)->getNumOperands(); op++)
		{
			MDString *mdstr = dyn_cast<MDString>(struct_md->getOperand(0)->getOperand(op));
			assert(mdstr != nullptr);
			if (st->getName().equals(mdstr->getString()))
			{
				errs() << st->getName() << " is of struct type\n";
				(*struct2Type)[st] = "struct";
				found = true;
				break;
			}
		}
		if(found)
			continue;
		for (auto op = 0u; op != header_union_md->getOperand(0)->getNumOperands(); op++)
		{
			MDString *mdstr = dyn_cast<MDString>(header_union_md->getOperand(0)->getOperand(op));
			assert(mdstr != nullptr);
			if (st->getName().equals(mdstr->getString()))
			{
				errs() << st->getName() << " is of header_union type\n";
				(*struct2Type)[st] = "header_union";
				found = true;
				break;
			}
		}
	}
}

void JsonBackend::printJsonToFile(const std::string filename)
{
	std::filebuf fb;
	fb.open(filename, std::ios::out);
	std::ostream os(&fb);
	// std::ostream *out = openFile(filename, false);
	jsonTop.serialize(os);
	errs() << "printed json to : " << filename << "\n";
}

void JsonBackend::populateJsonObjects(Module &M)
{
	jsonTop.emplace("program", M.getName());
	jsonTop.emplace("__meta__", json->meta);
	jsonTop.emplace("header_types", json->header_types);
	jsonTop.emplace("headers", json->headers);
	jsonTop.emplace("header_stacks", json->header_stacks);
	jsonTop.emplace("header_union_types", json->header_union_types);
	jsonTop.emplace("header_unions", json->header_unions);
	jsonTop.emplace("header_union_stacks", json->header_union_stacks);
	field_lists = LLBMV2::mkArrayField(&jsonTop, "field_lists");
	jsonTop.emplace("errors", json->errors);
	jsonTop.emplace("enums", json->enums);
	jsonTop.emplace("parsers", json->parsers);
	jsonTop.emplace("deparsers", json->deparsers);
	meter_arrays = LLBMV2::mkArrayField(&jsonTop, "meter_arrays");
	counters = LLBMV2::mkArrayField(&jsonTop, "counter_arrays");
	register_arrays = LLBMV2::mkArrayField(&jsonTop, "register_arrays");
	jsonTop.emplace("calculations", json->calculations);
	learn_lists = LLBMV2::mkArrayField(&jsonTop, "learn_lists");
	LLBMV2::nextId("learn_lists");
	jsonTop.emplace("actions", json->actions);
	jsonTop.emplace("pipelines", json->pipelines);
	jsonTop.emplace("checksums", json->checksums);
	force_arith = LLBMV2::mkArrayField(&jsonTop, "force_arith");
	jsonTop.emplace("extern_instances", json->externs);
	jsonTop.emplace("field_aliases", json->field_aliases);
}

bool JsonBackend::emitHeaders(Function *F) {
	// Get function arguments
	// Emit headers recursively for each argument
	if(F->getAttributes().getFnAttributes().hasAttribute("parser")) {
		errs() << "Found parser function\n";
		for(auto param = F->arg_begin(); param != F->arg_end(); param++) {
			auto st = dyn_cast<StructType>(dyn_cast<PointerType>((&*param)->getType())->getElementType());
			if (st != nullptr && (*struct2Type)[st] == "struct")
			{
				errs() << "Calling parser function\n";
				ch.processParams(st, struct2Type, json);
			}
			else {
				errs() << "not calling processParams\n";
				errs() << (*struct2Type)[st] << "\n";
			}
		}
		// Padding is calulated for scalars, add it to json now.
		ch.addScalarPadding(json);
	}
    return true;
}

extern "C" {
    ModulePass *createJsonBackendPass()
    {
    return new JsonBackend();
    }
}