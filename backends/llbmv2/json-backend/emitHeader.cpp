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

#include "emitHeader.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;
using namespace LLVMJsonBackend;

namespace LLBMV2 {

Util::JsonArray* ConvertHeaders::pushNewArray(Util::JsonArray* parent) {
    auto result = new Util::JsonArray();
    parent->append(result);
    return result;
}

ConvertHeaders::ConvertHeaders(){
    insertInMetaMap();
}

/**
 * Create header type and header instance from a IR::StructLike type
 *
 * @param meta this boolean indicates if the struct is a metadata or header.
 */
void ConvertHeaders::addTypesAndInstances(llvm::StructType *type,
                std::map<llvm::StructType *, std::string> *struct2Type,
                JsonObjects *json, bool meta) {
    for (auto f : type->elements()) {
        if (f->isStructTy()) {
                // The headers struct can not contain nested structures.
            auto st = dyn_cast<StructType>(f);
            if (!meta && (*struct2Type)[st] == "struct" ) {
                errs() << "ERROR : "  << st->getName()
                    << "should only contain headers, header stacks, or header unions\n";
                return;
            }
            addHeaderType(st, struct2Type, json);
        }
    }

    unsigned fieldCount = 0;
    for (auto f : type->elements()) {
        if (f->isStructTy()) {
            auto ft = dyn_cast<StructType>(f);
            auto header_name = type->getName().str() + ".field_" +
                                std::to_string(fieldCount++);
            auto header_type = ft->getName().str();
            if (meta == true) {
                json->add_metadata(header_type, header_name);
            } else {
                assert(ft != nullptr && "This should never happern");
                if ((*struct2Type)[ft] == "header") {
                    json->add_header(header_type, header_name);
                    setHeaderType(header_name, header_type, Header);
                } else if ((*struct2Type)[ft] == "header_union") {
                    // We have to add separately a header instance for all
                    // headers in the union.  Each instance will be named with
                    // a prefix including the union name, e.g., "u.h"
                    Util::JsonArray* fields = new Util::JsonArray();
                    unsigned unionFieldCount = 0;
                    for (auto uf : ft->elements()) {
                        auto h_name = header_name + "." + ft->getName().str()
                                        + ".field_" + std::to_string(unionFieldCount++);
                        auto h_type = dyn_cast<StructType>(uf)->getName().str();
                        unsigned id = json->add_header(h_type, h_name);
                        setHeaderType(h_name, h_type, Header);
                        fields->append(id);
                    }
                    json->add_union(header_type, fields, header_name);
                    setHeaderType(header_name, header_type, Union);
                } else {
                    errs() << "Unexpected type " << ft->getName() << "\n";
                    exit(1);
                }
            }
        } else if (f->isArrayTy() && f->getArrayElementType()->isStructTy()) {
            // Header stack type done elsewhere
            // errs() << "stack generation done elsewhere\n";
            continue;
        } else {
            // Treat this field like a scalar local variable
            cstring newName = type->getName().str() + ".field_" + std::to_string(fieldCount++);
            if (f->isIntegerTy(1)) {
                addHeaderField(json, scalarsTypeName, newName, boolWidth, false);
                if(!setHeaderType(newName, scalarsTypeName, Scalar))
                    assert(false && "Header name cannot be added to headerMap");
                scalars_width += boolWidth;
                // backend->scalarMetadataFields.emplace(f, newName);
            } else if (f->isIntegerTy()) {
                unsigned bitWidth = f->getIntegerBitWidth();
                addHeaderField(json, scalarsTypeName, newName, bitWidth, true);
                if (!setHeaderType(newName, scalarsTypeName, Scalar))
                    assert(false && "Header name cannot be added to headerMap");
                scalars_width += bitWidth;
                // backend->scalarMetadataFields.emplace(f, newName);
            } else if (f->isVectorTy() && f->getVectorElementType()->isIntegerTy(1)) {
                unsigned bitWidth = f->getVectorNumElements();
                addHeaderField(json, scalarsTypeName, newName, bitWidth, false);
                if (!setHeaderType(newName, scalarsTypeName, Scalar))
                    assert(false && "Header name cannot be added to headerMap");
                scalars_width += bitWidth;
                // backend->scalarMetadataFields.emplace(f, newName);
            // } else if (f->is<IR::Type_Error>()) {
            //     addHeaderField(scalarsTypeName, newName, errorWidth, 0);
            //     scalars_width += errorWidth;
            //     backend->scalarMetadataFields.emplace(f, newName);
            } else {
                // BUG("%1%: Unhandled type for %2%", ft, f);
                errs() << f->getTypeID() << " Unhandled type\n";
                exit(1);
            }
        }
    }
}

void ConvertHeaders::addHeaderStacks(llvm::StructType *headersStruct,
                                    std::map<llvm::StructType *, std::string> *struct2Type,
                                    JsonObjects *json)
{
    unsigned fieldCount = 0;
    for (auto f : headersStruct->elements()) {
        if(!f->isArrayTy())
            continue;
        else if(f->isArrayTy() && !f->getArrayElementType()->isStructTy()) {
            errs() << "BUG : " << f->getArrayElementType()->getTypeID()
                    << " not a struct type\n";
            exit(1);
        }
        auto stack_name = headersStruct->getName().str() + ".field_" + std::to_string(fieldCount++);
        auto stack_size = f->getArrayNumElements();
        auto ht = dyn_cast<StructType>(f->getArrayElementType());
        assert(ht != nullptr && "This shouldn not happen");
        if((*struct2Type)[ht] != "header") {
            errs() << "BUG : " << f->getArrayElementType()->getTypeID()
                   << " not a header type\n";
            exit(1);
        }
        assert((*struct2Type)[ht] == "header" && "Should be of header type only");
        addHeaderType(ht, struct2Type, json);
        cstring stack_type = ht->getName().str();
        std::vector<unsigned> ids;
        for (unsigned i = 0; i < stack_size; i++) {
            cstring hdrName = ht->getName().str() + "[" + std::to_string(i) + "]";
            auto id = json->add_header(stack_type, hdrName);
            setHeaderType(hdrName, stack_type, Header);
            ids.push_back(id);
        }
        json->add_header_stack(stack_type, stack_name, stack_size, ids);
    }
}

bool ConvertHeaders::isHeaders(llvm::StructType *st,
                std::map<llvm::StructType *, std::string> *struct2Type) {
    bool result = false;
    for (auto f : st->elements()) {
        if(f->isStructTy() && (*struct2Type)[dyn_cast<StructType>(f)] == "header")
            result = true;
        else if (f->isArrayTy() && f->getArrayElementType()->isStructTy()) {
            result = true;
        }
    }
    return result;
}

void ConvertHeaders::addHeaderField(JsonObjects *json, const cstring& header, const cstring& name,
                                    int size, bool is_signed) {
    Util::JsonArray* field = new Util::JsonArray();
    field->append(name);
    field->append(size);
    field->append(is_signed);
    json->add_header_field(header, field);
}

void ConvertHeaders::addHeaderType(llvm::StructType *st,
                                   std::map<llvm::StructType *, std::string> *struct2Type,
                                   JsonObjects *json)
{
    cstring name = (cstring)st->getName();
    if(name == "struct.standard_metadata_t")
        name = "standard_metadata";

    auto fields = new Util::JsonArray();
    unsigned max_length = 0;  // for variable-sized headers
    bool varbitFound = false;
    unsigned fieldCount = 0; // This used to name the fields as LLVM IR doesn't preserve struct field names

    if ((*struct2Type)[st] == "header_union") {
        for (auto f : st->elements()) {
            // fieldCount = 0;
            // auto ftype = typeMap->getType(f, true);
            llvm::StructType* ht = dyn_cast<llvm::StructType>(f);
            if((*struct2Type)[ht] != "header")
                assert(false && "Header_union has non-header type");
            addHeaderType(ht, struct2Type, json);
            auto field = pushNewArray(fields);
            // field->append(f->getName());
            field->append(st->getName().str() + ".field_"
                            + std::to_string(fieldCount++));
            field->append(ht->getName());
        }
        json->add_union_type(st->getName().str(), fields);
        return;
    }

    fieldCount = 0;
    for (auto f : st->elements()) {
        if (f->isStructTy()) {
            llvm::errs() <<"ERROR : " << dyn_cast<StructType>(f)->getName() << " : nested structure\n";
            assert(false);
            exit(1);
        } else if (f->isIntegerTy(1)) { // Bool Type
            auto field = pushNewArray(fields);
            auto name = st->getName().str() + ".field_" + std::to_string(fieldCount++);
            field->append(name);
            field->append(boolWidth);
            field->append(false);
            max_length += boolWidth;
        } else if (f->isIntegerTy()) { // Integet Type
            auto field = pushNewArray(fields);
            auto name = st->getName().str() + ".field_" + std::to_string(fieldCount++);
            field->append(name);
            field->append(f->getIntegerBitWidth());
            field->append(true);
            max_length += f->getIntegerBitWidth();
        } else if (f->isVectorTy() && f->getVectorElementType()->isIntegerTy(1)) { // bits<> Type : is considerd as Array of 1 bits
            auto field = pushNewArray(fields);
            auto name = (cstring)(st->getName().str() + ".field_" + std::to_string(fieldCount++));
            field->append(getFromMetaMap(name));
            field->append(f->getVectorNumElements());
            field->append(false);
            max_length += f->getVectorNumElements();
        } else if (f->isArrayTy() && f->getArrayElementType()->isStructTy()) {
            errs() << st->getName() << "nested stack : abnormal exit\n";
            exit(1);
        } else {
            errs() << f->getTypeID()
                    << " : unexpected type in "
                    << st->getName().str()
                    << ".field_" << std::to_string(fieldCount) << "\n";
            exit(1);
        }
    }

    // must add padding
    unsigned padding = max_length % 8;
    if (padding != 0) {
        cstring name = "_padding";
        auto field = pushNewArray(fields);
        field->append(name);
        field->append(8 - padding);
        field->append(false);
    }

    unsigned max_length_bytes = (max_length + padding) / 8;
    if (!varbitFound) {
        // ignore
        max_length = 0;
        max_length_bytes = 0;
    }
    json->add_header_type(name, fields, max_length_bytes);
}

/**
 * We synthesize a "header_type" for each local which has a struct type
 * and we pack all the scalar-typed locals into a 'scalar' type
 */
void ConvertHeaders::processHeaders(llvm::SmallVector<llvm::AllocaInst *, 8> *allocaList,
                                    std::map<llvm::StructType *, std::string> *struct2Type,
                                    JsonObjects *json)
{
    scalarsTypeName = genName("scalars_");
    json->add_header_type(scalarsTypeName);
    // bit<n>, bool, error are packed into scalars type,
    // varbit, struct and stack introduce new header types
    // for (auto v : backend->getStructure().variables) {
    unsigned allocaCount = 0;
    std::string allocaName;
    for(auto v : *allocaList) {
        Type* type = v->getAllocatedType();
        if (type->isStructTy()) {
            llvm::StructType* st = dyn_cast<llvm::StructType>(type);
            // assert(st != nullptr && "Alloca is not of Struct type");
            // auto metadata_type = st->controlPlaneName();
            auto metadata_type = st->getName().str();
            if ((*struct2Type)[st] == "header") {
                allocaName = st->getName().str() + "_"+ std::to_string(allocaCount++);
                json->add_header(metadata_type, allocaName);
                setHeaderType(allocaName, metadata_type, Header);
            }
            else {
                allocaName = st->getName().str() + "_" + std::to_string(allocaCount++);
                json->add_metadata(metadata_type, allocaName);
            }
            addHeaderType(st, struct2Type, json);
        } else if (type->isArrayTy() && type->getArrayElementType()->isStructTy()) {
            // Handle header stack
            // assert(false && "Header_stack is not yet handled");
            auto ht = dyn_cast<StructType>(type->getArrayElementType());
            assert((*struct2Type)[ht] == "header" && "not a header type");
            addHeaderType(ht, struct2Type, json);
            cstring header_type = ht->getName().str();
            std::vector<unsigned> header_ids;
            for (unsigned i=0; i < type->getArrayNumElements(); i++) {
                cstring name = header_type + "[" + std::to_string(i) + "]";
                auto header_id = json->add_header(header_type, name);
                setHeaderType(name, header_type, Header);
                header_ids.push_back(header_id);
            }
            std::string allocaName = header_type+"_"+std::to_string(allocaCount++);
            json->add_header_stack(header_type, allocaName,
                                    type->getArrayNumElements(), header_ids);
        // } else if (type->is<IR::Type_Varbits>()) {
        //     // For each varbit variable we synthesize a separate header instance,
        //     // since we cannot have multiple varbit fields in a single header.
        //     auto vbt = type->to<IR::Type_Varbits>();
        //     cstring headerName = "$varbit" + Util::toString(vbt->size);  // This name will be unique
        //     auto vec = new IR::IndexedVector<IR::StructField>();
        //     auto sf = new IR::StructField("field", type);
        //     vec->push_back(sf);
        //     typeMap->setType(sf, type);
        //     auto hdrType = new IR::Type_Header(headerName, *vec);
        //     typeMap->setType(hdrType, hdrType);
        //     json->add_metadata(headerName, v->name);
        //     if (visitedHeaders.find(headerName) != visitedHeaders.end())
        //         continue;  // already seen
        //     visitedHeaders.emplace(headerName);
        //     addHeaderType(hdrType);
        } else if (type->isVectorTy() && type->getVectorElementType()->isIntegerTy(1)) {
            // The above is the condition of checking `bits` type
            unsigned nBits = type->getVectorNumElements();
            allocaName =  "scalars.alloca_" + std::to_string(allocaCount++);
            addHeaderField(json, scalarsTypeName, allocaName, nBits, false);
            if (!setHeaderType(allocaName, scalarsTypeName, Scalar))
                assert(false && "Header name cannot be added to headerMap");
            scalars_width += nBits;
        } else if (type->isIntegerTy(1)) {
            allocaName =  "scalars.alloca_" + std::to_string(allocaCount++);
            addHeaderField(json, scalarsTypeName, allocaName, boolWidth, false);
            if (!setHeaderType(allocaName, scalarsTypeName, Scalar))
                assert(false && "Header name cannot be added to headerMap");
            scalars_width += boolWidth;
        // } else if (type->is<IR::Type_Error>()) {
        //     addHeaderField(scalarsTypeName, v->name.name, errorWidth, 0);
        //     scalars_width += errorWidth;
        } else if (type->isPointerTy() && type->getPointerElementType()->isFunctionTy()) {
            continue;
        } else {
            // P4C_UNIMPLEMENTED("%1%: type not yet handled on this target", type);
            if(v->getParent()->getParent()->getAttributes().getFnAttributes().hasAttribute("action"))
                continue;
            errs() << "inst is : " << *v << "\n";
            errs() << type->getTypeID() << " : type is not handled\n";
            assert(false && "Type not handled");
        }
        setAllocaName(v, allocaName);
    }

    // always-have metadata instance
    // json->add_metadata(scalarsTypeName, scalarsName);
    json->add_metadata(scalarsTypeName, "scalars");
    json->add_metadata("standard_metadata", "standard_metadata");
}

// void ConvertHeaders::end_apply(const IR::Node*) {
//     // pad scalars to byte boundary
//     unsigned padding = scalars_width % 8;
//     if (padding != 0) {
//         cstring name = refMap->newName("_padding");
//         addHeaderField(scalarsTypeName, name, 8-padding, false);
//     }
// }

/**
 * Generate json for header from IR::Block's constructor parameters
 *
 * The only allowed fields in a struct are: Type_Bits, Type_Bool and Type_Header
 *
 * header H {
 *   bit<32> x;
 * }
 *
 * struct {
 *   bit<32> x;
 *   bool    y;
 *   H       h;
 * }
 *
 * Type_Struct within a Type_Struct, i.e. nested struct, should be flattened apriori.
 *
 * @pre assumes no nested struct in parameters.
 * @post none
 */
bool ConvertHeaders::processParams(llvm::StructType *st,
                std::map<llvm::StructType *, std::string> *struct2Type,
                JsonObjects *json)
{
    //// keep track of which headers we've already generated the json for
    if(std::find(visitedHeaders.begin(), visitedHeaders.end(),
                st->getName().str()) != visitedHeaders.end())
        return false;  // already seen
    else
        visitedHeaders.push_back(st->getName().str());

    if (st->getName().str() == "struct.standard_metadata_t") {
        addHeaderType(st, struct2Type, json);
    }
    else {
        auto isHeader = isHeaders(st, struct2Type);
        if (isHeader) {
            addTypesAndInstances(st, struct2Type, json, false);
            addHeaderStacks(st, struct2Type, json);
        } else {
            addTypesAndInstances(st, struct2Type, json, true);
        }
    }
    return false;
}

}  // namespace LLBMV2
