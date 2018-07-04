/*
IITH Compilers
authors: S Venkata Keerthy, D Tharun
email: {cs17mtech11018, cs15mtech11002}@iith.ac.in

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

#include "ir/ir.h"
#include "backend.h"
#include "header.h"

namespace LLBMV2 {

ConvertHeaders::ConvertHeaders(Backend* backend)
        : backend(backend), refMap(backend->getRefMap()),
          typeMap(backend->getTypeMap()) {
    setName("ConvertHeaders");
}

/**
 * Create header type and header instance from a IR::StructLike type
 *
 * @param meta this boolean indicates if the struct is a metadata or header.
 */
void ConvertHeaders::addTypesAndInstances(const IR::Type_StructLike* type, bool meta) {
    for (auto f : type->fields) {
        auto ft = typeMap->getType(f, true);
        if (ft->is<IR::Type_StructLike>()) {
            // The headers struct can not contain nested structures.
            if (!meta && ft->is<IR::Type_Struct>()) {
                ::error("Type %1% should only contain headers, header stacks, or header unions",
                        type);
                return;
            }
            auto st = ft->to<IR::Type_StructLike>();
            addHeaderType(st);
        }
    }

    for (auto f : type->fields) {
        auto ft = typeMap->getType(f, true);
        if (ft->is<IR::Type_StructLike>()) {
            if (meta != true && !ft->is<IR::Type_Header>() && !ft->is<IR::Type_HeaderUnion>()) {
                BUG("Unexpected type %1%", ft);
            }
        } else if (ft->is<IR::Type_Stack>()) {
            // Done elsewhere
            LOG1("stack generation done elsewhere");
            continue;
        } else {
            // Treat this field like a scalar local variable
            cstring newName = refMap->newName(type->getName() + "." + f->name);
            if (ft->is<IR::Type_Bits>()) {
                auto tb = ft->to<IR::Type_Bits>();
                scalars_width += tb->size;
                backend->scalarMetadataFields.emplace(f, newName);
            } else if (ft->is<IR::Type_Boolean>()) {
                scalars_width += boolWidth;
                backend->scalarMetadataFields.emplace(f, newName);
            } else if (ft->is<IR::Type_Error>()) {
                scalars_width += errorWidth;
                backend->scalarMetadataFields.emplace(f, newName);
            } else {
                BUG("%1%: Unhandled type for %2%", ft, f);
            }
        }
    }
}

void ConvertHeaders::addHeaderStacks(const IR::Type_Struct* headersStruct) {
    LOG1("Creating stack " << headersStruct);
    for (auto f : headersStruct->fields) {
        auto ft = typeMap->getType(f, true);
        auto stack = ft->to<IR::Type_Stack>();
        if (stack == nullptr)
            continue;
        auto type = typeMap->getTypeType(stack->elementType, true);
        BUG_CHECK(type->is<IR::Type_Header>(), "%1% not a header type", stack->elementType);
        auto ht = type->to<IR::Type_Header>();
        addHeaderType(ht);
    }
}

bool ConvertHeaders::isHeaders(const IR::Type_StructLike* st) {
    bool result = false;
    for (auto f : st->fields) {
        if (f->type->is<IR::Type_Header>() || f->type->is<IR::Type_Stack>()) {
            result = true;
        }
    }
    return result;
}

void ConvertHeaders::addHeaderType(const IR::Type_StructLike *st) {
    bool varbitFound = false;
    if (st->is<IR::Type_HeaderUnion>()) {
        for (auto f : st->fields) {
            auto ftype = typeMap->getType(f, true);
            auto ht = ftype->to<IR::Type_Header>();
            CHECK_NULL(ht);
            addHeaderType(ht);
        }
        return;
    }

    for (auto f : st->fields) {
        auto ftype = typeMap->getType(f, true);
        if (ftype->to<IR::Type_StructLike>()) {
            BUG("%1%: nested structure", st);
        } else if (ftype->is<IR::Type_Boolean>()) {
        } else if (ftype->to<IR::Type_Bits>()) {
        } else if (ftype->to<IR::Type_Varbits>()) {
            if (varbitFound)
                ::error("%1%: headers with multiple varbit fields not supported", st);
            varbitFound = true;
        } else if (ftype->to<IR::Type_Stack>()) {
            BUG("%1%: nested stack", st);
        } else {
            BUG("%1%: unexpected type for %2%.%3%", ftype, st, f->name);
        }
    }
}

/**
 * We synthesize a "header_type" for each local which has a struct type
 * and we pack all the scalar-typed locals into a 'scalar' type
 */
Visitor::profile_t ConvertHeaders::init_apply(const IR::Node* node) {
    // bit<n>, bool, error are packed into scalars type,
    // varbit, struct and stack introduce new header types
    for (auto v : backend->getStructure().variables) {
        LOG1("variable " << v);
        auto type = typeMap->getType(v, true);
        if (auto st = type->to<IR::Type_StructLike>()) {
            addHeaderType(st);
        } else if (auto stack = type->to<IR::Type_Stack>()) {
            auto type = typeMap->getTypeType(stack->elementType, true);
            BUG_CHECK(type->is<IR::Type_Header>(), "%1% not a header type", stack->elementType);
            auto ht = type->to<IR::Type_Header>();
            addHeaderType(ht);
        } else if (type->is<IR::Type_Varbits>()) {
            // For each varbit variable we synthesize a separate header instance,
            // since we cannot have multiple varbit fields in a single header.
            auto vbt = type->to<IR::Type_Varbits>();
            cstring headerName = "$varbit" + Util::toString(vbt->size);  // This name will be unique
            auto vec = new IR::IndexedVector<IR::StructField>();
            auto sf = new IR::StructField("field", type);
            vec->push_back(sf);
            typeMap->setType(sf, type);
            auto hdrType = new IR::Type_Header(headerName, *vec);
            typeMap->setType(hdrType, hdrType);
            if (visitedHeaders.find(headerName) != visitedHeaders.end())
                continue;  // already seen
            visitedHeaders.emplace(headerName);
            addHeaderType(hdrType);
        } else if (type->is<IR::Type_Bits>()) {
            auto tb = type->to<IR::Type_Bits>();
            scalars_width += tb->size;
        } else if (type->is<IR::Type_Boolean>()) {
            scalars_width += boolWidth;
        } else if (type->is<IR::Type_Error>()) {
            scalars_width += errorWidth;
        } else {
            P4C_UNIMPLEMENTED("%1%: type not yet handled on this target", type);
        }
    }
    return Inspector::init_apply(node);
}

void ConvertHeaders::end_apply(const IR::Node*) {
}

bool ConvertHeaders::preorder(const IR::Parameter* param) {
    //// keep track of which headers we've already generated the json for
    auto ft = typeMap->getType(param->getNode(), true);
    if (ft->is<IR::Type_Struct>()) {
        auto st = ft->to<IR::Type_Struct>();
        if (visitedHeaders.find(st->getName()) != visitedHeaders.end())
            return false;  // already seen
        else
            visitedHeaders.emplace(st->getName());

        if (st->getAnnotation("metadata")) {
            addHeaderType(st);
            // visit(st);
        } else {
            auto isHeader = isHeaders(st);
            if (isHeader) {
                addTypesAndInstances(st, false);
                addHeaderStacks(st);
            } else {
                addTypesAndInstances(st, true);
            }
        }
    }
    return false;
}

//  Blocks are not in IR tree, use a custom visitor to traverse
bool ConvertHeaders::preorder(const IR::PackageBlock *block) {
    for (auto it : block->constantValue) {
        if (it.second->is<IR::Type_StructLike>()) {
            visit(it.second->getNode());
        }
    }
    return false;
}

bool ConvertHeaders::preorder(const IR::Type_StructLike* t) {
    auto z = backend->defined_type[t->name];
    if(z)
        return backend->defined_type[t->name];
   
    std::vector<Type*> members;
    for(auto x: t->fields)
        members.push_back(backend->getCorrespondingType(x->type)); // for int and all

    StructType *structReg = llvm::StructType::create(backend->TheContext, members, "struct."+t->name); //  

    if(t->is<IR::Type_Struct>())
        backend->structMDV.push_back(MDString::get(backend->TheContext, "struct."+t->name));
    else if(t->is<IR::Type_Header>())
        backend->headerMDV.push_back(MDString::get(backend->TheContext, "struct."+t->name));
    else if(t->is<IR::Type_HeaderUnion>())
        backend->huMDV.push_back(MDString::get(backend->TheContext, "struct."+t->name));

    backend->defined_type[t->name] = structReg;

    int i=0;
    for(auto x: t->fields) {
        backend->structIndexMap[structReg][std::string(x->name.name)] = i;
        i++;
    }
    return false;
}

}  // namespace LLBMV2
