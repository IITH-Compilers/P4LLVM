/*
Copyright 2013-present Barefoot Networks, Inc.

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

/// constant definition for LLBMV2
// const cstring TableImplementation::actionProfileName = "action_profile";
// const cstring TableImplementation::actionSelectorName = "action_selector";
// const cstring TableImplementation::directCounterName = "direct_counter";
// const cstring TableImplementation::counterName = "counter";
// const cstring TableImplementation::directMeterName = "direct_meter";
// const cstring MatchImplementation::selectorMatchTypeName = "selector";
// const cstring MatchImplementation::rangeMatchTypeName = "range";
// const cstring TableAttributes::implementationName = "implementation";
// const cstring TableAttributes::sizeName = "size";
// const cstring TableAttributes::supportTimeoutName = "supportTimeout";
// const cstring TableAttributes::countersName = "counters";
// const cstring TableAttributes::metersName = "meters";
// const unsigned TableAttributes::defaultTableSize = 1024;
// const cstring V1ModelProperties::jsonMetadataParameterName = "standard_metadata";
// const cstring V1ModelProperties::validField = "$valid$";

// Util::IJson* nodeName(const CFG::Node* node) {
//     if (node->name.isNullOrEmpty())
//         return Util::JsonValue::null;
//     else
//         return new Util::JsonValue(node->name);
// }

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
    std::string result;
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
    return result.substr(0, result.length() - 2);
}

cstring getFieldName(Value *arg, Util::JsonArray *fieldArr)
{
    //errs() << "input inst is \n"
        //    << *arg << "\n";
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
        //errs() << "No of operands in gep : " << gep->getNumOperands() << "\n";
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
            std::string result = getMultiDimFieldName(gep, fieldArr);
            return result + getFieldName(gep->getPointerOperand(), fieldArr);
        }
    }

    if (auto ld = dyn_cast<LoadInst>(arg))
    {
        //errs() << "No of operands in load : " << ld->getNumOperands() << "\n";
        return getFieldName(ld->getOperand(0), fieldArr);
    }

    if (auto bc = dyn_cast<BitCastInst>(arg))
    {
        //errs() << "No of operands in bitcast : " << bc->getNumOperands() << "\n";
        assert(bc->getType()->isPointerTy() && "not a pointer type in getFieldName");
        return getFieldName(bc->getOperand(0), fieldArr);
    }
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
    // if(metaMap.find(name) == metaMap.end())
    //     metaMap[name] = meta_name;
    // else
    //     assert(false && "Name alredy exists in metaMap");
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
        // assert(false && "Name doesn't exit in metaMap");
}

bool isSMeta(cstring name) {
    if (metaMap.find(name) != metaMap.end())
        return true;
    else return false;
}

}  // namespace LLBMV2