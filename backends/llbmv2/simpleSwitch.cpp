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

/**
 * This file implements the simple switch model
 */

#include <algorithm>
#include <cstring>
#include <set>
#include "frontends/p4/fromv1.0/v1model.h"
#include "backends/bmv2/backend.h"
#include "simpleSwitch.h"

using LLBMV2::nextId;
using LLBMV2::stringRepr;

namespace P4V1 {

// need a class to generate simpleSwitch


void
SimpleSwitch::modelError(const char* format, const IR::Node* node) const {
    ::error(format, node);
    ::error("Are you using an up-to-date v1model.p4?");
}


cstring
SimpleSwitch::convertHashAlgorithm(cstring algorithm) {
    cstring result;   
    if (algorithm == v1model.algorithm.crc32.name)
        result = "crc32";
    else if (algorithm == v1model.algorithm.crc32_custom.name)
        result = "crc32_custom";
    else if (algorithm == v1model.algorithm.crc16.name)
        result = "crc16";
    else if (algorithm == v1model.algorithm.crc16_custom.name)
        result = "crc16_custom";
    else if (algorithm == v1model.algorithm.random.name)
        result = "random";
    else if (algorithm == v1model.algorithm.identity.name)
        result = "identity";
    else
        ::error("%1%: unexpected algorithm", algorithm);
    return result;
}


void
SimpleSwitch::convertChecksum(const IR::BlockStatement *block,  bool verify) {
    if (errorCount() > 0)
        return;
    auto typeMap = backend->getTypeMap();
    auto refMap = backend->getRefMap();
    for (auto stat : block->components) {
        if (auto blk = stat->to<IR::BlockStatement>()) {
            convertChecksum(blk, verify);
            continue;
        } else if (auto mc = stat->to<IR::MethodCallStatement>()) {
            auto mi = P4::MethodInstance::resolve(mc, refMap, typeMap);
            if (auto em = mi->to<P4::ExternFunction>()) {
                cstring functionName = em->method->name.name;
                if ((verify && (functionName == v1model.verify_checksum.name ||
                                functionName == v1model.verify_checksum_with_payload.name)) ||
                    (!verify && (functionName == v1model.update_checksum.name ||
                                 functionName == v1model.update_checksum_with_payload.name))) {
                    if (mi->expr->arguments->size() != 4) {
                        modelError("%1%: Expected 4 arguments", mc);
                        return;
                    }
                    auto ei = P4::EnumInstance::resolve(mi->expr->arguments->at(3), typeMap);
                    if (ei->name != "csum16") {
                        ::error("%1%: the only supported algorithm is csum16",
                                mi->expr->arguments->at(3));
                        return;
                    }
                    continue;
                }
            }
        }
        ::error("%1%: Only calls to %2% or %3% allowed", stat,
                verify ? v1model.verify_checksum.name : v1model.update_checksum.name,
                verify ? v1model.verify_checksum_with_payload.name :
                v1model.update_checksum_with_payload.name);
    }
}

void
SimpleSwitch::setPipelineControls(const IR::ToplevelBlock* toplevel,
                                  std::set<cstring>* controls,
                                  std::map<cstring, cstring>* map) {
    if (errorCount() != 0)
        return;
    auto main = toplevel->getMain();
    if (main == nullptr) {
        ::error("`%1%' module not found for simple switch", IR::P4Program::main);
        return;
    }
    auto ingress = main->findParameterValue(v1model.sw.ingress.name);
    auto egress = main->findParameterValue(v1model.sw.egress.name);
    if (ingress == nullptr || egress == nullptr ||
        !ingress->is<IR::ControlBlock>() || !egress->is<IR::ControlBlock>()) {
        modelError("%1%: main package does not match the expected model", main);
        return;
    }
    auto ingress_name = ingress->to<IR::ControlBlock>()->container->name;
    auto egress_name = egress->to<IR::ControlBlock>()->container->name;
    controls->emplace(ingress_name);
    controls->emplace(egress_name);
    map->emplace(ingress_name, "ingress");
    map->emplace(egress_name, "egress");
}

const IR::P4Control* SimpleSwitch::getIngress(const IR::ToplevelBlock* blk) {
    auto main = blk->getMain();
    auto ctrl = main->findParameterValue(v1model.sw.ingress.name);
    if (ctrl == nullptr)
        return nullptr;
    if (!ctrl->is<IR::ControlBlock>()) {
        modelError("%1%: main package  match the expected model", main);
        return nullptr;
    }
    return ctrl->to<IR::ControlBlock>()->container;
}

const IR::P4Control* SimpleSwitch::getEgress(const IR::ToplevelBlock* blk) {
    auto main = blk->getMain();
    auto ctrl = main->findParameterValue(v1model.sw.egress.name);
    if (ctrl == nullptr)
        return nullptr;
    if (!ctrl->is<IR::ControlBlock>()) {
        modelError("%1%: main package  match the expected model", main);
        return nullptr;
    }
    return ctrl->to<IR::ControlBlock>()->container;
}

const IR::P4Parser* SimpleSwitch::getParser(const IR::ToplevelBlock* blk) {
    auto main = blk->getMain();
    auto ctrl = main->findParameterValue(v1model.sw.parser.name);
    if (ctrl == nullptr)
        return nullptr;
    if (!ctrl->is<IR::ParserBlock>()) {
        modelError("%1%: main package  match the expected model", main);
        return nullptr;
    }
    return ctrl->to<IR::ParserBlock>()->container;
}

void
SimpleSwitch::setNonPipelineControls(const IR::ToplevelBlock* toplevel,
                                     std::set<cstring>* controls) {
    if (errorCount() != 0)
        return;
    auto main = toplevel->getMain();
    auto verify = main->findParameterValue(v1model.sw.verify.name);
    auto update = main->findParameterValue(v1model.sw.update.name);
    auto deparser = main->findParameterValue(v1model.sw.deparser.name);
    if (verify == nullptr || update == nullptr || deparser == nullptr ||
        !verify->is<IR::ControlBlock>() || !update->is<IR::ControlBlock>() ||
        !deparser->is<IR::ControlBlock>()) {
        modelError("%1%: main package  match the expected model", main);
        return;
    }
    controls->emplace(verify->to<IR::ControlBlock>()->container->name);
    controls->emplace(update->to<IR::ControlBlock>()->container->name);
    controls->emplace(deparser->to<IR::ControlBlock>()->container->name);
}

void
SimpleSwitch::setUpdateChecksumControls(const IR::ToplevelBlock* toplevel,
                                        std::set<cstring>* controls) {
    if (errorCount() != 0)
        return;
    auto main = toplevel->getMain();
    auto update = main->findParameterValue(v1model.sw.update.name);
    if (update == nullptr || !update->is<IR::ControlBlock>()) {
        modelError("%1%: main package does not match the expected model", main);
        return;
    }
    controls->emplace(update->to<IR::ControlBlock>()->container->name);
}

void
SimpleSwitch::setVerifyChecksumControls(const IR::ToplevelBlock* toplevel,
                                        std::set<cstring>* controls) {
    if (errorCount() != 0)
        return;
    auto main = toplevel->getMain();
    auto update = main->findParameterValue(v1model.sw.verify.name);
    if (update == nullptr || !update->is<IR::ControlBlock>()) {
        modelError("%1%: main package does not match the expected model", main);
        return;
    }
    controls->emplace(update->to<IR::ControlBlock>()->container->name);
}

void
SimpleSwitch::setDeparserControls(const IR::ToplevelBlock* toplevel,
                                  std::set<cstring>* controls) {
    auto main = toplevel->getMain();
    auto deparser = main->findParameterValue(v1model.sw.deparser.name);
    if (deparser == nullptr || !deparser->is<IR::ControlBlock>()) {
        modelError("%1%: main package does not match the expected model", main);
        return;
    }
    controls->emplace(deparser->to<IR::ControlBlock>()->container->name);
}

}  // namespace P4V1