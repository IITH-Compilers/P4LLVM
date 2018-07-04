/*
IITH Compilers
authors: D Tharun, S Venkata Keerthy
email: {cs15mtech11002, cs17mtech11018}@iith.ac.in

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

#include "helpers.h"

using namespace llvm;
namespace LLVMJsonBackend {

Util::JsonArray* mkArrayField(Util::JsonObject* parent, cstring name) {
    auto result = new Util::JsonArray();
    parent->emplace(name, result);
    return result;
}

Util::JsonArray* mkParameters(Util::JsonObject* object) {
    return mkArrayField(object, "parameters");
}

Util::JsonArray* pushNewArray(Util::JsonArray* parent) {
    auto result = new Util::JsonArray();
    parent->append(result);
    return result;
}

Util::JsonObject* mkPrimitive(cstring name, Util::JsonArray* appendTo) {
    auto result = new Util::JsonObject();
    result->emplace("op", name);
    appendTo->append(result);
    return result;
}

unsigned nextId(cstring group) {
    static std::map<cstring, unsigned> counters;
    return counters[group]++;
}

bool setAllocaName(llvm::Instruction *I, std::string name)
{
    if (allocaMap.find(I) == allocaMap.end())
    {
        allocaMap[I] = name;
        return true;
    }
    else
    {
        return false;
    }
}

std::string getAllocaName(llvm::Instruction *I)
{
    if (allocaMap.find(I) != allocaMap.end())
    {
        return allocaMap[I];
    }
    else
    {
        assert(false && "Alloca name is not in allocaMap");
        return "";
    }
}

static bool isIndex1D(llvm::GetElementPtrInst *gep)
{
    if (gep->hasAllZeroIndices())
        return true;
    if (gep->getNumIndices() > 2)
        return false;
    return true;
}

static unsigned get1DIndex(llvm::GetElementPtrInst *gep)
{
    for (auto id = gep->idx_begin(); id != gep->idx_end(); id++)
    {
        unsigned val = dyn_cast<ConstantInt>(id)->getZExtValue();
        //errs() << "1D index : " << val << "\n";
        if (val != 0)
            return val;
    }
    return 0;
}

static std::string getMultiDimFieldName(llvm::GetElementPtrInst *gep, Util::JsonArray* fieldArr)
{
    auto id = gep->idx_begin();
    id++; // ignore the first index as it will be a zero corresponding to the current type
    std::string result = ".";
    auto src_type = gep->getSourceElementType();
    for (; id != gep->idx_end(); id++)
    {
        unsigned val = dyn_cast<ConstantInt>(id)->getZExtValue();
        if (isa<StructType>(src_type)) {
            auto interimName = dyn_cast<StructType>(src_type)->getName().str() + ".field_" + std::to_string(val);
            if(fieldArr)
                fieldArr->append(interimName);
            result = result + interimName + ".";
        }
        else
            assert(false && "This should not happen");
        src_type = dyn_cast<StructType>(src_type)->getElementType(val);
    }
    return result.substr(0, result.length() - 1);
}

cstring getFieldName(Value *arg, Util::JsonArray *fieldArr)
{
    // This condition is for getting action parameter name
    if(auto fun_arg = dyn_cast<Argument>(arg))
        if(!fun_arg->getType()->isPointerTy()) {
            auto paramName = (fun_arg->getParent()->getName() + "._" + fun_arg->getName());
            return paramName.str().c_str();
        }
    if (!isa<Instruction>(arg))
        return "";
    if (isa<AllocaInst>(arg)) {
        auto retName = getAllocaName(dyn_cast<Instruction>(arg));
        if(fieldArr)
            fieldArr->append(retName);
        return retName;
    }

    auto gep = dyn_cast<GetElementPtrInst>(arg);
    if (gep)
    {
        if (isIndex1D(gep))
        {
            auto src_type = dyn_cast<StructType>(gep->getSourceElementType());
            assert(src_type != nullptr && "This should not happen");
            auto retName = getFieldName(gep->getPointerOperand(), fieldArr) + "." + src_type->getName().str().c_str() + ".field_" + std::to_string(get1DIndex(gep)).c_str();
            // standard_metadata is always 1D gep
            if(isSMeta(retName.substr(1))) {
                retName = getFromMetaMap(retName.substr(1));
                if (fieldArr) {
                    fieldArr->append("standard_metadata");
                    fieldArr->append(retName);
                }
                return retName;
            }
            if(fieldArr)
                fieldArr->append(src_type->getName().str().c_str() + cstring(".field_") + std::to_string(get1DIndex(gep)).c_str());
            return retName;
        }
        else
        {
            auto result = getMultiDimFieldName(gep, fieldArr);
            return result + getFieldName(gep->getPointerOperand(), fieldArr);
        }
    }

    if (auto ld = dyn_cast<LoadInst>(arg))
    {
        return getFieldName(ld->getOperand(0), fieldArr);
    }

    if (auto bc = dyn_cast<BitCastInst>(arg))
    {
        assert(bc->getType()->isPointerTy() && "not a pointer type in getFieldName");
        // This type is for checking if there is cast from header to integer type, this may happen
        // if we are accessing first element of the structure.
        // In this case get the first element not the first structure
        if(bc->getType()->getPointerElementType()->isIntegerTy() &&
            bc->getOperand(0)->getType()->getPointerElementType()->isStructTy()) {

            auto elemt = dyn_cast<StructType>(bc->getOperand(0)->getType()->getPointerElementType());
            auto headerName = getFieldName(bc->getOperand(0), fieldArr) + elemt->getName().str() + ".field_0";
            if(fieldArr)
                fieldArr->append(elemt->getName().str() + ".field_0");
            while(elemt->getElementType(0)->isStructTy()) {
                elemt = dyn_cast<StructType>(elemt->getElementType(0));
                headerName = headerName + "." + elemt->getName().str()+".field_0";
                if(fieldArr)
                    fieldArr->append(elemt->getName().str() + ".field_0");
            }
            return (headerName).c_str();
        }
        return getFieldName(bc->getOperand(0), fieldArr);
    }

    if (auto iv = dyn_cast<InsertValueInst>(arg))
    {
        // auto gep = dyn_cast<GetElementPtrInst>(dyn_cast<Instruction>(iv->getOperand(0))->getOperand(0));
        // if(!gep)
        //     assert(false && "insetvalue inst has no gep instruction");
        return ("."+StringRef(getFieldName(iv->getOperand(1), fieldArr).substr(1)).split(".struct").first).str().c_str();
    }
    // assert(false && "Unknow type instruction type in getFieldName");
    return "";
}

cstring getHeaderType(cstring headerType, header_type ht) {
    switch(ht) {
        case Scalar:
            if (headerMap[0].find(headerType) != headerMap[0].end())
                return headerMap[0][headerType];
            else
                return std::string("").c_str();
        case Header:
            if (headerMap[1].find(headerType) != headerMap[1].end())
                return headerMap[1][headerType];
            else
                return std::string("").c_str();
        case Union:
            if (headerMap[2].find(headerType) != headerMap[2].end())
                return headerMap[2][headerType];
            else
                return std::string("").c_str();
        default:
            return std::string("").c_str();
    }
}

bool setHeaderType(cstring headername, cstring headerType, header_type ht) {
    switch(ht) {
        case Scalar:
            if (headerMap[0].find(headername) == headerMap[0].end()) {
                headerMap[0][headername] = headerType;
                return true;
            }
            else
                return false;
        case Header:
            if (headerMap[1].find(headername) == headerMap[1].end()) {
                headerMap[1][headername] = headerType;
                return true;
            }
            else
                return false;
        case Union:
            if (headerMap[2].find(headername) == headerMap[2].end()) {
                headerMap[2][headername] = headerType;
                return true;
            }
            else
                return false;
        default:
            return false;
    }
}

header_type getBasicHeaderType(cstring headerName) {
    for(unsigned i : {0, 1, 2}) {
        if(headerMap[i].find(headerName.c_str()) != headerMap[i].end())
            return static_cast<header_type>(i);
    }
}

cstring genName(cstring name)
{
    if (genNameMap.find(name) == genNameMap.end())
    {
        genNameMap[name] = 0;
    }
    return name + std::to_string(genNameMap[name]++);
}

unsigned getActionID(cstring action)
{
    if(actionIDMap.find(action) != actionIDMap.end())
        return actionIDMap[action];
    else {
        assert(false && "Action id not there in action map");
    }
    return 0;
}

void setActionID(cstring action, unsigned id)
{
    if (actionIDMap.find(action) == actionIDMap.end())
        actionIDMap[action] = id;
    else {
        errs() << action << "\n";
        assert(false && "setActionID called with same action more than once");
    }
}

void insertInMetaMap(){
    metaMap["struct.standard_metadata_t.field_0"] = "ingress_port";
    metaMap["struct.standard_metadata_t.field_1"] = "egress_spec";
    metaMap["struct.standard_metadata_t.field_2"] = "egress_port";
    metaMap["struct.standard_metadata_t.field_3"] = "clone_spec";
    metaMap["struct.standard_metadata_t.field_4"] = "instance_type";
    metaMap["struct.standard_metadata_t.field_5"] = "drop";
    metaMap["struct.standard_metadata_t.field_6"] = "recirculate_port";
    metaMap["struct.standard_metadata_t.field_7"] = "packet_length";
    metaMap["struct.standard_metadata_t.field_8"] = "enq_timestamp";
    metaMap["struct.standard_metadata_t.field_9"] = "enq_qdepth";
    metaMap["struct.standard_metadata_t.field_10"] = "deq_timedelta";
    metaMap["struct.standard_metadata_t.field_11"] = "deq_qdepth";
    metaMap["struct.standard_metadata_t.field_12"] = "ingress_global_timestamp";
    metaMap["struct.standard_metadata_t.field_13"] = "lf_field_list";
    metaMap["struct.standard_metadata_t.field_14"] = "mcast_grp";
    metaMap["struct.standard_metadata_t.field_15"] = "resubmit_flag";
    metaMap["struct.standard_metadata_t.field_16"] = "egress_rid";
    metaMap["struct.standard_metadata_t.field_17"] = "checksum_error";
    metaMap["struct.standard_metadata_t.field_18"] = "_padding";
}

cstring getFromMetaMap(cstring name)
{
    if(metaMap.find(name) != metaMap.end())
        return metaMap[name];
    else
        return name;
}

bool isSMeta(cstring name) {
    if (metaMap.find(name) != metaMap.end())
        return true;
    else return false;
}

void emitErrors(LLBMV2::JsonObjects* json) {
    pushNewArray(json->errors)->append("NoError")->append(0);
    pushNewArray(json->errors)->append("PacketTooShort")->append(1);
    pushNewArray(json->errors)->append("NoMatch")->append(2);
    pushNewArray(json->errors)->append("StackOutOfBounds")->append(3);
    pushNewArray(json->errors)->append("HeaderTooShort")->append(4);
    pushNewArray(json->errors)->append("ParserTimeout")->append(5);
}

void emitFieldAliases(LLBMV2::JsonObjects* json) {
    pushNewArray(json->field_aliases)->append("queueing_metadata.enq_timestamp")
            ->append((new Util::JsonArray())->append("standard_metadata")
                                            ->append("enq_timestamp"));
    pushNewArray(json->field_aliases)->append("queueing_metadata.enq_qdepth")
                ->append((new Util::JsonArray())->append("standard_metadata")
                                                ->append("enq_qdepth"));
    pushNewArray(json->field_aliases)->append("queueing_metadata.deq_timedelta")
                    ->append((new Util::JsonArray())->append("standard_metadata")
                                                    ->append("deq_timedelta"));
    pushNewArray(json->field_aliases)->append("queueing_metadata.deq_qdepth")
                ->append((new Util::JsonArray())->append("standard_metadata")
                                                ->append("deq_qdepth"));
    pushNewArray(json->field_aliases)->append("intrinsic_metadata.ingress_global_timestamp")
                ->append((new Util::JsonArray())->append("standard_metadata")
                                                ->append("ingress_global_timestamp"));
    pushNewArray(json->field_aliases)->append("intrinsic_metadata.lf_field_list")
                ->append((new Util::JsonArray())->append("standard_metadata")
                                                ->append("lf_field_list"));
    pushNewArray(json->field_aliases)->append("intrinsic_metadata.mcast_grp")
                ->append((new Util::JsonArray())->append("standard_metadata")
                                                ->append("mcast_grp"));
    pushNewArray(json->field_aliases)->append("intrinsic_metadata.resubmit_flag")
                ->append((new Util::JsonArray())->append("standard_metadata")
                                                ->append("resubmit_flag"));
    pushNewArray(json->field_aliases)->append("intrinsic_metadata.egress_rid")
                ->append((new Util::JsonArray())->append("standard_metadata")
                                                ->append("egress_rid"));
}

cstring getOpcodeSym(cstring op) {
    if(op == "add")
        return "+";
    else if(op == "mul")
        return "*";
    else if(op == "sub")
        return "-";
    else if(op == "sdiv")
        return "/";
    else if(op == "srem")
        return "%";
    else if (op == "shl")
        return "<<";
    else if (op == "shr")
        return ">>";
    else if(op == "and")
        return "&";
    else if(op == "or")
        return "|";
    else if(op == "xor")
        return "^";
    else
        assert(false && "unknown opcode");
    return "";
}

Util::IJson *getJsonExp(Value *inst)
{
    //errs() << "inst into getJsonExp is\n" << *inst << "\n";
    auto result = new Util::JsonObject();
    if (auto bo = dyn_cast<BinaryOperator>(inst))
    {
        cstring op = getOpcodeSym(bo->getOpcodeName());
        result->emplace("op", op);
        auto left = getJsonExp(bo->getOperand(0));
        auto right = getJsonExp(bo->getOperand(1));
        result->emplace("left", left);
        result->emplace("right", right);
        // auto fin = new Util::JsonObject();
        assert(inst->getType()->isIntegerTy() && "should be an integer type");
        unsigned bw = inst->getType()->getIntegerBitWidth();
        auto result_ex = new Util::JsonObject();
        result_ex->emplace("type", "expression");
        result_ex->emplace("value", result);
        std::stringstream stream;
        stream << "0x" << std::setfill('0') << std::setw(bw/4) << std::hex << ((1 << bw) - 1);
        auto trunc = new Util::JsonObject();
        trunc->emplace("op", "&");
        trunc->emplace("left", result_ex);
        auto trunc_val = new Util::JsonObject();
        trunc_val->emplace("type", "hexstr");
        trunc_val->emplace("value", stream.str().c_str());
        trunc->emplace("right", trunc_val);
        auto trunc_exp = new Util::JsonObject();
        trunc_exp->emplace("type", "expression");
        trunc_exp->emplace("value", trunc);
        // auto trunc_exp_exp = new Util::JsonObject();
        // trunc_exp_exp->emplace("type", "expression");
        // trunc_exp_exp->emplace("value", trunc_exp);
        return trunc_exp;
    }
    else if (auto sel = dyn_cast<SelectInst>(inst))
    {
        if (sel->getType()->isIntegerTy(1))
        {
            result->emplace("op", "?");
            auto left = dyn_cast<ConstantInt>(sel->getOperand(1))->getZExtValue();
            auto right = dyn_cast<ConstantInt>(sel->getOperand(2))->getZExtValue();
            auto cond = getJsonExp(sel->getOperand(0));
            auto left_exp = new Util::JsonObject();
            left_exp->emplace("type", "hexstr");
            left_exp->emplace("value", left);
            auto right_exp = new Util::JsonObject();
            right_exp->emplace("type", "hexstr");
            right_exp->emplace("value", right);
            result->emplace("left", left_exp);
            result->emplace("right", right_exp);
            result->emplace("cond", cond);
            auto result_ex = new Util::JsonObject();
            result_ex->emplace("type", "expression");
            result_ex->emplace("value", result);
            return result_ex;
        }
        else
            assert(false && "select inst with non boolean not handled");
    }
    else if (auto cmp = dyn_cast<ICmpInst>(inst))
    {
        //errs() << "number of operands in icmp are : " << cmp->getNumOperands() << "\n";
        auto pred = CmpInst::getPredicateName(cmp->getSignedPredicate()).str();
        //errs() << "predicate name: " << pred << "\n";
        cstring op;
        if (pred == "eq")
            op = "==";
        else if (pred == "ne")
            op = "!=";
        else if (pred == "sge" || pred == "uge")
            op = ">=";
        else if (pred == "sle" || pred == "ule")
            op = "<=";
        else if (pred == "sgt" || pred == "ugt")
            op = ">";
        else if (pred == "slt" || pred == "ult")
            op = "<";
        else
        {
            errs() << *inst << "\n";
            assert(false && "Unknown operation in ICMP");
        }
        auto left = getJsonExp(cmp->getOperand(0));
        auto right = getJsonExp(cmp->getOperand(1));
        result->emplace("op", op);
        result->emplace("left", left);
        result->emplace("right", right);
        auto result_ex = new Util::JsonObject();
        result_ex->emplace("type", "expression");
        result_ex->emplace("value", result);
        return result_ex;
    }
    else if (auto cnst = dyn_cast<ConstantInt>(inst))
    {
        std::stringstream stream;
        stream << "0x" << std::setfill('0') << std::setw(cnst->getBitWidth() / 4)
               << std::hex << (cnst->getSExtValue() & ((1 << cnst->getBitWidth())-1));
        result->emplace("type", "hexstr");
        result->emplace("value", stream.str().c_str());
        return result;
    }
    else if (auto ld = dyn_cast<LoadInst>(inst))
    {
        auto headername = new Util::JsonArray();
        getFieldName(ld->getOperand(0), headername);
        result->emplace("type", "field");
        result->emplace("value", headername);
        return result;
    }
    else if (auto bc = dyn_cast<BitCastInst>(inst))
    {
        return getJsonExp(bc->getOperand(0));
    }
    else if (auto ze = dyn_cast<ZExtInst>(inst))
    {
        auto left = getJsonExp(ze->getOperand(0));
        auto bw = ze->getType()->getIntegerBitWidth();
        result->emplace("op", "&");
        result->emplace("left", left);
        std::stringstream stream;
        stream << "0x" << std::setfill('0') << std::setw(bw / 4) << std::hex << ((1 << bw) - 1);
        auto trunc_val = new Util::JsonObject();
        trunc_val->emplace("type", "hexstr");
        trunc_val->emplace("value", stream.str().c_str());
        result->emplace("right", trunc_val);
        auto result_ex = new Util::JsonObject();
        result_ex->emplace("type", "expression");
        result_ex->emplace("value", result);
        return result_ex;
    }
    else if(auto fun_arg = dyn_cast<Argument>(inst))
    {
        return Util::JsonValue::null;
    }
    else
    {
        errs() << *inst << "\n"
               << "ERROR : Unhandled instrution in getJsonExp\n";
        assert(false);
        return result;
    }
}

}  // namespace LLBMV2
