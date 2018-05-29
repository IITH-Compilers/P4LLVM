#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include <vector>
#include "../JsonObjects.h"
// #include "../helpers.h"
#include "lib/json.h"
#include "emitHeader.h"
#include <fstream>
#include <iostream>

using namespace llvm;

namespace
{
struct JsonBackend : public ModulePass
{
public:
    static char ID;
		JsonBackend() : ModulePass(ID) {
			json = new LLBMV2::JsonObjects();
		}

		virtual bool runOnModule(Module &M)
    {
			// initialize json objects
			populateJsonObjects(M);
			auto structs = M.getIdentifiedStructTypes();
			NamedMDNode *header_md = M.getNamedMetadata("header");
			NamedMDNode *struct_md = M.getNamedMetadata("struct");
			NamedMDNode *header_union_md = M.getNamedMetadata("header_union");
			if (header_md->getOperand(0)->getNumOperands() != 0)
				emitHeaderTypes(structs, header_md);
			// errs() << "No of struct types are: " << structs.size() << "\n";
			// if(struct_md != nullptr) {
			//     errs() << "No of operands in struct : " << struct_md->getNumOperands() << "\n";
			//     errs() << "operand: " << dyn_cast<MDString>(struct_md->getOperand(0)->getOperand(0))->getString() << "\n";
			// }
			// else
			//     errs() << "struct_md is null\n";

			// if (header_md != nullptr) {
			//     errs() << "No of operands in header : " << header_md->getNumOperands() << "\n";
			//     errs() << "operand: " << header_md->getOperand(0)->getNumOperands() << "\n";
			// }
			// else
			//     errs() << "header_md is null\n";

			// if (header_union_md != nullptr) {
			//     errs() << "No of operands in header_union : " << header_union_md->getNumOperands() << "\n";
			//     errs() << "operand: " << header_union_md->getOperand(0)->getNumOperands() << "\n";
			// }
			// else
			//     errs() << "header_union_md is null\n";

			// for(auto structtype : structs) {
			//     errs() << "Name of the struct is :" << structtype->getName() << "\n";
			// }
			// Iterate on functions
			for (auto fun = M.begin(); fun != M.end(); fun++)
			{
				errs() << "Name of the function is: " << (&*fun)->getName() << "\n";
				runOnFunction(&*fun);
      }
			printJsonToFile(M.getSourceFileName()+".json");
			return false;
    }
    bool runOnFunction(Function *F);
    bool emitHeaderTypes(std::vector<StructType *>&, NamedMDNode *);
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
		LLBMV2::ConvertHeaders ch;
		void printJsonToFile(const std::string fn);
};
}

char JsonBackend::ID = 0;

void JsonBackend::printJsonToFile(const std::string filename) {
	std::filebuf fb;
	fb.open(filename, std::ios::out);
	std::ostream os(&fb);
	// std::ostream *out = openFile(filename, false);
	jsonTop.serialize(os);
	errs() << "printed json to : " << filename << "\n";
}

Util::JsonArray *mkArrayField(Util::JsonObject *parent, cstring name)
{
	auto result = new Util::JsonArray();
	parent->emplace(name, result);
	return result;
}

unsigned nextId(cstring group)
{
	static std::map<cstring, unsigned> counters;
	return counters[group]++;
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
	field_lists = mkArrayField(&jsonTop, "field_lists");
	jsonTop.emplace("errors", json->errors);
	jsonTop.emplace("enums", json->enums);
	jsonTop.emplace("parsers", json->parsers);
	jsonTop.emplace("deparsers", json->deparsers);
	meter_arrays = mkArrayField(&jsonTop, "meter_arrays");
	counters = mkArrayField(&jsonTop, "counter_arrays");
	register_arrays = mkArrayField(&jsonTop, "register_arrays");
	jsonTop.emplace("calculations", json->calculations);
	learn_lists = mkArrayField(&jsonTop, "learn_lists");
	nextId("learn_lists");
	jsonTop.emplace("actions", json->actions);
	jsonTop.emplace("pipelines", json->pipelines);
	jsonTop.emplace("checksums", json->checksums);
	force_arith = mkArrayField(&jsonTop, "force_arith");
	jsonTop.emplace("extern_instances", json->externs);
	jsonTop.emplace("field_aliases", json->field_aliases);
}

bool JsonBackend::emitHeaderTypes(std::vector<StructType *>& structs, NamedMDNode *header_md) {
		assert(header_md->getNumOperands() == 1 && "Header namedMetadata should have only one operand.");
    for(auto st: structs) {
			for (auto op = 0; op != header_md->getOperand(0)->getNumOperands(); op++) {
				MDString *mdstr = dyn_cast<MDString>(header_md->getOperand(0)->getOperand(op));
				assert(mdstr != nullptr);
				if (st->getName().equals(mdstr->getString())) {
					errs() << st->getName() << " is of Header type\n";
					ch.addHeaderType(st, json);
					errs() << "successfully added a header\n";
				}
			}
		}
}

bool JsonBackend::runOnFunction(Function *F) {
    return true;
}

extern "C" {
    ModulePass *createJsonBackendPass()
    {
    return new JsonBackend();
    }
}