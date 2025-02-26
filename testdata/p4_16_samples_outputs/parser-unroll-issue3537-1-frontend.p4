#include <core.p4>
#define V1MODEL_VERSION 20180101
#include <v1model.p4>

@command_line("--loopsUnroll") struct H {
}

struct M {
}

parser ParserI(packet_in packet, out H hdr, inout M meta, inout standard_metadata_t smeta) {
    @name("ParserI.tmp") bit<16> tmp;
    @name("ParserI.tmp_0") bit<16> tmp_0;
    @name("ParserI.tmp_1") bit<16> tmp_1;
    @name("ParserI.tmp_2") bit<16> tmp_2;
    state start {
        transition s1;
    }
    state s1 {
        packet.advance(32w16);
        tmp_0 = packet.lookahead<bit<16>>();
        tmp = tmp_0;
        transition select(tmp) {
            16w0: s1;
            default: s2;
        }
    }
    state s2 {
        packet.advance(32w16);
        tmp_2 = packet.lookahead<bit<16>>();
        tmp_1 = tmp_2;
        transition select(tmp_1) {
            16w0: s1;
            16w1: s2;
            default: s4;
        }
    }
    state s4 {
        transition accept;
    }
}

control IngressI(inout H hdr, inout M meta, inout standard_metadata_t smeta) {
    apply {
    }
}

control EgressI(inout H hdr, inout M meta, inout standard_metadata_t smeta) {
    apply {
    }
}

control DeparserI(packet_out pk, in H hdr) {
    apply {
    }
}

control VerifyChecksumI(inout H hdr, inout M meta) {
    apply {
    }
}

control ComputeChecksumI(inout H hdr, inout M meta) {
    apply {
    }
}

V1Switch<H, M>(ParserI(), VerifyChecksumI(), IngressI(), EgressI(), ComputeChecksumI(), DeparserI()) main;
