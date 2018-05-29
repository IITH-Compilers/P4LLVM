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

// #include "ir/ir.h"
// #include "backend.h"
#include "emitHeader.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace LLBMV2 {

// TODO(hanw): remove
Util::JsonArray* ConvertHeaders::pushNewArray(Util::JsonArray* parent) {
    auto result = new Util::JsonArray();
    parent->append(result);
    return result;
}

ConvertHeaders::ConvertHeaders(){
    // setName("ConvertHeaders");
}

/**
 * Create header type and header instance from a IR::StructLike type
 *
 * @param meta this boolean indicates if the struct is a metadata or header.
 */
// void ConvertHeaders::addTypesAndInstances(const IR::Type_StructLike* type, bool meta) {
//     LOG1("Adding " << type);
//     for (auto f : type->fields) {
//         auto ft = typeMap->getType(f, true);
//         if (ft->is<IR::Type_StructLike>()) {
//             // The headers struct can not contain nested structures.
//             if (!meta && ft->is<IR::Type_Struct>()) {
//                 ::error("Type %1% should only contain headers, header stacks, or header unions",
//                         type);
//                 return;
//             }
//             auto st = ft->to<IR::Type_StructLike>();
//             addHeaderType(st);
//         }
//     }

//     for (auto f : type->fields) {
//         auto ft = typeMap->getType(f, true);
//         if (ft->is<IR::Type_StructLike>()) {
//             auto header_name = f->controlPlaneName();
//             auto header_type = ft->to<IR::Type_StructLike>()->controlPlaneName();
//             if (meta == true) {
//                 json->add_metadata(header_type, header_name);
//             } else {
//                 if (ft->is<IR::Type_Header>()) {
//                     json->add_header(header_type, header_name);
//                 } else if (ft->is<IR::Type_HeaderUnion>()) {
//                     // We have to add separately a header instance for all
//                     // headers in the union.  Each instance will be named with
//                     // a prefix including the union name, e.g., "u.h"
//                     Util::JsonArray* fields = new Util::JsonArray();
//                     for (auto uf : ft->to<IR::Type_HeaderUnion>()->fields) {
//                         auto uft = typeMap->getType(uf, true);
//                         auto h_name = header_name + "." + uf->controlPlaneName();
//                         auto h_type = uft->to<IR::Type_StructLike>()
//                                          ->controlPlaneName();
//                         unsigned id = json->add_header(h_type, h_name);
//                         fields->append(id);
//                     }
//                     json->add_union(header_type, fields, header_name);
//                 } else {
//                     BUG("Unexpected type %1%", ft);
//                 }
//             }
//         } else if (ft->is<IR::Type_Stack>()) {
//             // Done elsewhere
//             LOG1("stack generation done elsewhere");
//             continue;
//         } else {
//             // Treat this field like a scalar local variable
//             cstring newName = refMap->newName(type->getName() + "." + f->name);
//             if (ft->is<IR::Type_Bits>()) {
//                 auto tb = ft->to<IR::Type_Bits>();
//                 addHeaderField(scalarsTypeName, newName, tb->size, tb->isSigned);
//                 scalars_width += tb->size;
//                 backend->scalarMetadataFields.emplace(f, newName);
//             } else if (ft->is<IR::Type_Boolean>()) {
//                 addHeaderField(scalarsTypeName, newName, boolWidth, 0);
//                 scalars_width += boolWidth;
//                 backend->scalarMetadataFields.emplace(f, newName);
//             } else if (ft->is<IR::Type_Error>()) {
//                 addHeaderField(scalarsTypeName, newName, errorWidth, 0);
//                 scalars_width += errorWidth;
//                 backend->scalarMetadataFields.emplace(f, newName);
//             } else {
//                 BUG("%1%: Unhandled type for %2%", ft, f);
//             }
//         }
//     }
// }

// void ConvertHeaders::addHeaderStacks(const IR::Type_Struct* headersStruct) {
//     LOG1("Creating stack " << headersStruct);
//     for (auto f : headersStruct->fields) {
//         auto ft = typeMap->getType(f, true);
//         auto stack = ft->to<IR::Type_Stack>();
//         if (stack == nullptr)
//             continue;
//         auto stack_name = f->controlPlaneName();
//         auto stack_size = stack->getSize();
//         auto type = typeMap->getTypeType(stack->elementType, true);
//         BUG_CHECK(type->is<IR::Type_Header>(), "%1% not a header type", stack->elementType);
//         auto ht = type->to<IR::Type_Header>();
//         addHeaderType(ht);
//         cstring stack_type = stack->elementType->to<IR::Type_Header>()
//                                                ->controlPlaneName();
//         std::vector<unsigned> ids;
//         for (unsigned i = 0; i < stack_size; i++) {
//             cstring hdrName = f->controlPlaneName() + "[" + Util::toString(i) + "]";
//             auto id = json->add_header(stack_type, hdrName);
//             ids.push_back(id);
//         }
//         json->add_header_stack(stack_type, stack_name, stack_size, ids);
//     }
// }

// bool ConvertHeaders::isHeaders(const IR::Type_StructLike* st) {
//     bool result = false;
//     for (auto f : st->fields) {
//         if (f->type->is<IR::Type_Header>() || f->type->is<IR::Type_Stack>()) {
//             result = true;
//         }
//     }
//     return result;
// }

void ConvertHeaders::addHeaderField(JsonObjects *json, const cstring& header, const cstring& name,
                                    int size, bool is_signed) {
    Util::JsonArray* field = new Util::JsonArray();
    field->append(name);
    field->append(size);
    field->append(is_signed);
    json->add_header_field(header, field);
}

void ConvertHeaders::addHeaderType(const llvm::StructType *st, JsonObjects *json)
{
    // cstring name = st->controlPlaneName();
    cstring name = (cstring)st->getName();
    auto fields = new Util::JsonArray();
    unsigned max_length = 0;  // for variable-sized headers
    bool varbitFound = false;
    unsigned fieldCount = 0; // This used to name the fields as LLVM IR doesn't preserve struct field names

    for (auto f : st->elements()) {
        if (f->isStructTy()) {
            llvm::errs() << st->getName() << " : nested structure\n";
            exit(1);
        } else if (f->isIntegerTy() && f->getIntegerBitWidth() > 1) { // Integet Type
            auto field = pushNewArray(fields);
            field->append(st->getName() + ".field" + std::to_string(fieldCount++));
            field->append(f->getIntegerBitWidth());
            field->append(true);
            max_length += f->getIntegerBitWidth();
        } else if (f->isIntegerTy() && f->getIntegerBitWidth() == 1) { // Bool Type
            auto field = pushNewArray(fields);
            field->append(st->getName()+".field"+std::to_string(fieldCount++));
            field->append(boolWidth);
            field->append(false);
            max_length += boolWidth;
        } else if (f->isArrayTy()) { // bits<> Type : is considerd as Array of 1 bits
            auto field = pushNewArray(fields);
            field->append(st->getName() + ".field" + std::to_string(fieldCount++));
            field->append(f->getArrayNumElements());
            field->append(false);
            max_length += f->getArrayNumElements();
        } else if (f->isStructTy()) {
            errs() << st->getName() << "nested stack : abnormal exit\n";
            exit(1);
        } else {
            errs() << f->getTypeID() << " : unexpected type for " << st->getName() << ".field" << std::to_string(fieldCount++) << "\n";
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
// void ConvertHeaders::processStructs(std::map<llvm::StructType *, std::string>* struct2Type) {
//     scalarsTypeName = "scalars";
//     json->add_header_type(scalarsTypeName);
//     // bit<n>, bool, error are packed into scalars type,
//     // varbit, struct and stack introduce new header types
//     for (auto v : backend->getStructure().variables) {
//         LOG1("variable " << v);
//         auto type = typeMap->getType(v, true);
//         if (auto st = type->to<IR::Type_StructLike>()) {
//             auto metadata_type = st->controlPlaneName();
//             if (type->is<IR::Type_Header>())
//                 json->add_header(metadata_type, v->name);
//             else
//                 json->add_metadata(metadata_type, v->name);
//             addHeaderType(st);
//         } else if (auto stack = type->to<IR::Type_Stack>()) {
//             auto type = typeMap->getTypeType(stack->elementType, true);
//             BUG_CHECK(type->is<IR::Type_Header>(), "%1% not a header type", stack->elementType);
//             auto ht = type->to<IR::Type_Header>();
//             addHeaderType(ht);
//             cstring header_type = stack->elementType->to<IR::Type_Header>()
//                                                     ->controlPlaneName();
//             std::vector<unsigned> header_ids;
//             for (unsigned i=0; i < stack->getSize(); i++) {
//                 cstring name = v->name + "[" + Util::toString(i) + "]";
//                 auto header_id = json->add_header(header_type, name);
//                 header_ids.push_back(header_id);
//             }
//             json->add_header_stack(header_type, v->name, stack->getSize(), header_ids);
//         } else if (type->is<IR::Type_Varbits>()) {
//             // For each varbit variable we synthesize a separate header instance,
//             // since we cannot have multiple varbit fields in a single header.
//             auto vbt = type->to<IR::Type_Varbits>();
//             cstring headerName = "$varbit" + Util::toString(vbt->size);  // This name will be unique
//             auto vec = new IR::IndexedVector<IR::StructField>();
//             auto sf = new IR::StructField("field", type);
//             vec->push_back(sf);
//             typeMap->setType(sf, type);
//             auto hdrType = new IR::Type_Header(headerName, *vec);
//             typeMap->setType(hdrType, hdrType);
//             json->add_metadata(headerName, v->name);
//             if (visitedHeaders.find(headerName) != visitedHeaders.end())
//                 continue;  // already seen
//             visitedHeaders.emplace(headerName);
//             addHeaderType(hdrType);
//         } else if (type->is<IR::Type_Bits>()) {
//             auto tb = type->to<IR::Type_Bits>();
//             addHeaderField(scalarsTypeName, v->name.name, tb->size, tb->isSigned);
//             scalars_width += tb->size;
//         } else if (type->is<IR::Type_Boolean>()) {
//             addHeaderField(scalarsTypeName, v->name.name, boolWidth, false);
//             scalars_width += boolWidth;
//         } else if (type->is<IR::Type_Error>()) {
//             addHeaderField(scalarsTypeName, v->name.name, errorWidth, 0);
//             scalars_width += errorWidth;
//         } else {
//             P4C_UNIMPLEMENTED("%1%: type not yet handled on this target", type);
//         }
//     }

//     // always-have metadata instance
//     json->add_metadata(scalarsTypeName, scalarsName);
//     json->add_metadata("standard_metadata", "standard_metadata");
//     return Inspector::init_apply(node);
// }

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
// bool ConvertHeaders::preorder(const IR::Parameter* param) {
//     //// keep track of which headers we've already generated the json for
//     auto ft = typeMap->getType(param->getNode(), true);
//     if (ft->is<IR::Type_Struct>()) {
//         auto st = ft->to<IR::Type_Struct>();
//         if (visitedHeaders.find(st->getName()) != visitedHeaders.end())
//             return false;  // already seen
//         else
//             visitedHeaders.emplace(st->getName());

//         if (st->getAnnotation("metadata")) {
//             addHeaderType(st);
//         } else {
//             auto isHeader = isHeaders(st);
//             if (isHeader) {
//                 addTypesAndInstances(st, false);
//                 addHeaderStacks(st);
//             } else {
//                 addTypesAndInstances(st, true);
//             }
//         }
//     }
//     return false;
// }

/// Blocks are not in IR tree, use a custom visitor to traverse
// bool ConvertHeaders::preorder(const IR::PackageBlock *block) {
//     for (auto it : block->constantValue) {
//         if (it.second->is<IR::Block>()) {
//             visit(it.second->getNode());
//         }
//     }
//     return false;
// }

}  // namespace LLBMV2
