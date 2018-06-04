/*
Copyright 2016 VMware, Inc.

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

#include <core.p4>
#include <v1model.p4>

header hdr {
    bit<32> f;
}

header abc {
    int<32> a;
    bit<16> b;
}

header_union hu {
    hdr h1;
    abc h2;
}

control compute(inout hdr h) {
    apply {
        hdr tmp;
        tmp.f = h.f + 1;
        h.f = tmp.f;
    }
}

#ifdef _CORE_P4_

struct Headers {
    hdr h;
    hu hu1;
    abc rk;
}

struct Meta {}

parser p(packet_in b, out Headers h, inout Meta m, inout standard_metadata_t sm) {
    bit<32> x = h.h.f;
    
    state start {
        abc sr;
        sr.a = 0;
        // h.rk.a = 0;
        b.extract(srirama);
        transition accept;
    }
}

control vrfy(inout Headers h, inout Meta m) { apply {} }
control update(inout Headers h, inout Meta m) { apply {} }

control egress(inout Headers h, inout Meta m, inout standard_metadata_t sm) { apply {} }

control deparser(packet_out b, in Headers h) {
    apply { b.emit(h.h); }
}

control ingress(inout Headers h, inout Meta m, inout standard_metadata_t sm) {
    compute() c;
    apply {
        c.apply(h.h);
        sm.egress_spec = 0;
    }
}

V1Switch(p(), vrfy(), ingress(), egress(), update(), deparser()) main;

#endif
