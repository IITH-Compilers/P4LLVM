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
namespace LLBMV2 {

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

// cstring stringRepr(mpz_class value, unsigned bytes) {
//     cstring sign = "";
//     const char* r;
//     cstring filler = "";
//     if (value < 0) {
//         value =- value;
//         r = mpz_get_str(nullptr, 16, value.get_mpz_t());
//         sign = "-";
//     } else {
//         r = mpz_get_str(nullptr, 16, value.get_mpz_t());
//     }

//     if (bytes > 0) {
//         int digits = bytes * 2 - strlen(r);
//         BUG_CHECK(digits >= 0, "Cannot represent %1% on %2% bytes", value, bytes);
//         filler = std::string(digits, '0');
//     }
//     return sign + "0x" + filler + r;
// }

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
        errs() << "1D index : " << val << "\n";
        if (val != 0)
            return val;
    }
    return 0;
}

static std::string getMultiDimFieldName(llvm::GetElementPtrInst *gep)
{
    auto id = gep->idx_begin();
    id++; // ignore the first index as it will be a zero corresponding to the current type
    std::string result;
    auto src_type = gep->getSourceElementType();
    for (; id != gep->idx_end(); id++)
    {
        unsigned val = dyn_cast<ConstantInt>(id)->getZExtValue();
        if (isa<StructType>(src_type))
            result = result + dyn_cast<StructType>(src_type)->getName().str() + ".field_" + std::to_string(val) + ".";
        else
            assert(false && "This should not happen");
        src_type = dyn_cast<StructType>(src_type)->getElementType(val);
    }
    return result.substr(0, result.length() - 2);
}

std::string getFieldName(Value *arg)
{
    errs() << "input inst is \n"
           << *arg << "\n";
    if (!isa<Instruction>(arg))
        return "";
    if (isa<AllocaInst>(arg))
        return "." + getAllocaName(dyn_cast<Instruction>(arg));
    auto gep = dyn_cast<GetElementPtrInst>(arg);
    if (gep)
    {
        errs() << "No of operands in gep : " << gep->getNumOperands() << "\n";
        if (isIndex1D(gep))
        {
            auto src_type = dyn_cast<StructType>(gep->getSourceElementType());
            assert(src_type != nullptr && "This should not happen");
            return getFieldName(gep->getPointerOperand()) + "." + src_type->getName().str() + ".field_" + std::to_string(get1DIndex(gep));
        }
        else
        {
            std::string result = getMultiDimFieldName(gep);
            return result + getFieldName(gep);
        }
    }

    if (auto ld = dyn_cast<LoadInst>(arg))
    {
        errs() << "No of operands in load : " << ld->getNumOperands() << "\n";
        return getFieldName(ld->getOperand(0));
    }

    if (auto bc = dyn_cast<BitCastInst>(arg))
    {
        errs() << "No of operands in bitcast : " << bc->getNumOperands() << "\n";
        return getFieldName(bc->getOperand(0));
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

}  // namespace LLBMV2


