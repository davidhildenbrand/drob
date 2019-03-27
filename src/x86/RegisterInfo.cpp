/*
 * This file is part of Drob.
 *
 * Copyright 2019 David Hildenbrand <davidhildenbrand@gmail.com>
 *
 * Drob is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * Drob is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * in the COPYING.LESSER files in the top-level directory for more details.
 */
#include "../arch.hpp"
#include "../RegisterInfo.hpp"
#include "gen/gen-defs.h"

namespace drob {

#define DEF_REG_SIMPLE(_reg, _type, _parent, _nr, _byteOffs) \
    static const RegisterInfo ri_##_reg = { \
        .type = RegisterType::_type, \
        .name = #_reg, \
        .nr = _nr, \
        .byteOffs = _byteOffs, \
        .parent = Register::_parent, \
        .full = { .m = { SUBREGISTER_MASK_##_reg } }, \
        .h = nullptr, \
        .f = nullptr, \
    }

#define DEF_REG_XMM(_reg) \
    static const SubRegisterMask rmh_XMM##_reg[2] = { \
        { .m = { SUBREGISTER_MASK_XMM##_reg##_Q0 } }, \
        { .m = { SUBREGISTER_MASK_XMM##_reg##_Q1 } }, \
    }; \
    static const SubRegisterMask rmf_XMM##_reg[4] = { \
        { .m = { SUBREGISTER_MASK_XMM##_reg##_D0 } }, \
        { .m = { SUBREGISTER_MASK_XMM##_reg##_D1 } }, \
        { .m = { SUBREGISTER_MASK_XMM##_reg##_D2 } }, \
        { .m = { SUBREGISTER_MASK_XMM##_reg##_D3 } }, \
    }; \
    static const RegisterInfo ri_XMM##_reg = { \
        .type = RegisterType::Sse128, \
        .name = "XMM"#_reg, \
        .nr = _reg, \
        .byteOffs = 0, \
        .parent = Register::None, \
        .full = { .m = { SUBREGISTER_MASK_XMM##_reg } }, \
        .h = rmh_XMM##_reg, \
        .f = rmf_XMM##_reg, \
    }


DEF_REG_SIMPLE(CF, Flag1, None, 0, 0);
DEF_REG_SIMPLE(PF, Flag1, None, 1, 0);
DEF_REG_SIMPLE(AF, Flag1, None, 2, 0);
DEF_REG_SIMPLE(ZF, Flag1, None, 3, 0);
DEF_REG_SIMPLE(SF, Flag1, None, 4, 0);
DEF_REG_SIMPLE(OF, Flag1, None, 5, 0);

DEF_REG_SIMPLE(RAX, Gprs64, None, 0, 0);
DEF_REG_SIMPLE(EAX, Gprs32, RAX, 0, 0);
DEF_REG_SIMPLE(AX, Gprs16, RAX, 0, 0);
DEF_REG_SIMPLE(AH, Gprs8, RAX, 0, 1);
DEF_REG_SIMPLE(AL, Gprs8, RAX, 0, 0);

DEF_REG_SIMPLE(RBX, Gprs64, None, 1, 0);
DEF_REG_SIMPLE(EBX, Gprs32, RBX, 1, 0);
DEF_REG_SIMPLE(BX, Gprs16, RBX, 1, 0);
DEF_REG_SIMPLE(BH, Gprs8, RBX, 1, 1);
DEF_REG_SIMPLE(BL, Gprs8, RBX, 1, 0);

DEF_REG_SIMPLE(RCX, Gprs64, None, 2, 0);
DEF_REG_SIMPLE(ECX, Gprs32, RCX, 2, 0);
DEF_REG_SIMPLE(CX, Gprs16, RCX, 2, 0);
DEF_REG_SIMPLE(CH, Gprs8, RCX, 2, 1);
DEF_REG_SIMPLE(CL, Gprs8, RCX, 2, 0);

DEF_REG_SIMPLE(RDX, Gprs64, None, 3, 0);
DEF_REG_SIMPLE(EDX, Gprs32, RDX, 3, 0);
DEF_REG_SIMPLE(DX, Gprs16, RDX, 3, 0);
DEF_REG_SIMPLE(DH, Gprs8, RDX, 3, 1);
DEF_REG_SIMPLE(DL, Gprs8, RDX, 3, 0);

DEF_REG_SIMPLE(RSI, Gprs64, None, 4, 0);
DEF_REG_SIMPLE(ESI, Gprs32, RSI, 4, 0);
DEF_REG_SIMPLE(SI, Gprs16, RSI, 4, 0);
DEF_REG_SIMPLE(SIL, Gprs8, RSI, 4, 0);

DEF_REG_SIMPLE(RDI, Gprs64, None, 5, 0);
DEF_REG_SIMPLE(EDI, Gprs32, RDI, 5, 0);
DEF_REG_SIMPLE(DI, Gprs16, RDI, 5, 0);
DEF_REG_SIMPLE(DIL, Gprs8, RDI, 5, 0);

DEF_REG_SIMPLE(RBP, Gprs64, None, 6, 0);
DEF_REG_SIMPLE(EBP, Gprs32, RBP, 6, 0);
DEF_REG_SIMPLE(BP, Gprs16, RBP, 6, 0);
DEF_REG_SIMPLE(BPL, Gprs8, RBP, 6, 0);

DEF_REG_SIMPLE(RSP, Gprs64, None, 7, 0);
DEF_REG_SIMPLE(ESP, Gprs32, RSP, 7, 0);
DEF_REG_SIMPLE(SP, Gprs16, RSP, 7, 0);
DEF_REG_SIMPLE(SPL, Gprs8, RSP, 7, 0);

DEF_REG_SIMPLE(R8, Gprs64, None, 8, 0);
DEF_REG_SIMPLE(R8D, Gprs32, R8, 8, 0);
DEF_REG_SIMPLE(R8W, Gprs16, R8, 8, 0);
DEF_REG_SIMPLE(R8B, Gprs8, R8, 8, 0);

DEF_REG_SIMPLE(R9, Gprs64, None, 9, 0);
DEF_REG_SIMPLE(R9D, Gprs32, R9, 9, 0);
DEF_REG_SIMPLE(R9W, Gprs16, R9, 9, 0);
DEF_REG_SIMPLE(R9B, Gprs8, R9, 9, 0);

DEF_REG_SIMPLE(R10, Gprs64, None, 10, 0);
DEF_REG_SIMPLE(R10D, Gprs32, R10, 10, 0);
DEF_REG_SIMPLE(R10W, Gprs16, R10, 10, 0);
DEF_REG_SIMPLE(R10B, Gprs8, R10, 10, 0);

DEF_REG_SIMPLE(R11, Gprs64, None, 11, 0);
DEF_REG_SIMPLE(R11D, Gprs32, R11, 11, 0);
DEF_REG_SIMPLE(R11W, Gprs16, R11, 11, 0);
DEF_REG_SIMPLE(R11B, Gprs8, R11, 11, 0);

DEF_REG_SIMPLE(R12, Gprs64, None, 12, 0);
DEF_REG_SIMPLE(R12D, Gprs32, R12, 12, 0);
DEF_REG_SIMPLE(R12W, Gprs16, R12, 12, 0);
DEF_REG_SIMPLE(R12B, Gprs8, R12, 12, 0);

DEF_REG_SIMPLE(R13, Gprs64, None, 13, 0);
DEF_REG_SIMPLE(R13D, Gprs32, R13, 13, 0);
DEF_REG_SIMPLE(R13W, Gprs16, R13, 13, 0);
DEF_REG_SIMPLE(R13B, Gprs8, R13, 13, 0);

DEF_REG_SIMPLE(R14, Gprs64, None, 14, 0);
DEF_REG_SIMPLE(R14D, Gprs32, R14, 14, 0);
DEF_REG_SIMPLE(R14W, Gprs16, R14, 14, 0);
DEF_REG_SIMPLE(R14B, Gprs8, R14, 14, 0);

DEF_REG_SIMPLE(R15, Gprs64, None, 15, 0);
DEF_REG_SIMPLE(R15D, Gprs32, R15, 15, 0);
DEF_REG_SIMPLE(R15W, Gprs16, R15, 15, 0);
DEF_REG_SIMPLE(R15B, Gprs8, R15, 15, 0);

DEF_REG_XMM(0);
DEF_REG_XMM(1);
DEF_REG_XMM(2);
DEF_REG_XMM(3);
DEF_REG_XMM(4);
DEF_REG_XMM(5);
DEF_REG_XMM(6);
DEF_REG_XMM(7);
DEF_REG_XMM(8);
DEF_REG_XMM(9);
DEF_REG_XMM(10);
DEF_REG_XMM(11);
DEF_REG_XMM(12);
DEF_REG_XMM(13);
DEF_REG_XMM(14);
DEF_REG_XMM(15);

#define REGISTER_INFO(_reg) \
    [static_cast<uint16_t>(Register::_reg)] = &ri_##_reg
#define REGISTER_INFO_NULL(_reg) \
    [static_cast<uint16_t>(Register::_reg)] = nullptr

const RegisterInfo *const ri[static_cast<uint16_t>(Register::MAX)] = {
    REGISTER_INFO_NULL(None),
    REGISTER_INFO(CF),
    REGISTER_INFO(PF),
    REGISTER_INFO(AF),
    REGISTER_INFO(ZF),
    REGISTER_INFO(SF),
    REGISTER_INFO(OF),
    REGISTER_INFO(RAX),
    REGISTER_INFO(EAX),
    REGISTER_INFO(AX),
    REGISTER_INFO(AH),
    REGISTER_INFO(AL),
    REGISTER_INFO(RBX),
    REGISTER_INFO(EBX),
    REGISTER_INFO(BX),
    REGISTER_INFO(BH),
    REGISTER_INFO(BL),
    REGISTER_INFO(RCX),
    REGISTER_INFO(ECX),
    REGISTER_INFO(CX),
    REGISTER_INFO(CH),
    REGISTER_INFO(CL),
    REGISTER_INFO(RDX),
    REGISTER_INFO(EDX),
    REGISTER_INFO(DX),
    REGISTER_INFO(DH),
    REGISTER_INFO(DL),
    REGISTER_INFO(RSI),
    REGISTER_INFO(ESI),
    REGISTER_INFO(SI),
    REGISTER_INFO(SIL),
    REGISTER_INFO(RDI),
    REGISTER_INFO(EDI),
    REGISTER_INFO(DI),
    REGISTER_INFO(DIL),
    REGISTER_INFO(RBP),
    REGISTER_INFO(EBP),
    REGISTER_INFO(BP),
    REGISTER_INFO(BPL),
    REGISTER_INFO(RSP),
    REGISTER_INFO(ESP),
    REGISTER_INFO(SP),
    REGISTER_INFO(SPL),
    REGISTER_INFO(R8),
    REGISTER_INFO(R8D),
    REGISTER_INFO(R8W),
    REGISTER_INFO(R8B),
    REGISTER_INFO(R9),
    REGISTER_INFO(R9D),
    REGISTER_INFO(R9W),
    REGISTER_INFO(R9B),
    REGISTER_INFO(R10),
    REGISTER_INFO(R10D),
    REGISTER_INFO(R10W),
    REGISTER_INFO(R10B),
    REGISTER_INFO(R11),
    REGISTER_INFO(R11D),
    REGISTER_INFO(R11W),
    REGISTER_INFO(R11B),
    REGISTER_INFO(R12),
    REGISTER_INFO(R12D),
    REGISTER_INFO(R12W),
    REGISTER_INFO(R12B),
    REGISTER_INFO(R13),
    REGISTER_INFO(R13D),
    REGISTER_INFO(R13W),
    REGISTER_INFO(R13B),
    REGISTER_INFO(R14),
    REGISTER_INFO(R14D),
    REGISTER_INFO(R14W),
    REGISTER_INFO(R14B),
    REGISTER_INFO(R15),
    REGISTER_INFO(R15D),
    REGISTER_INFO(R15W),
    REGISTER_INFO(R15B),
    REGISTER_INFO(XMM0),
    REGISTER_INFO(XMM1),
    REGISTER_INFO(XMM2),
    REGISTER_INFO(XMM3),
    REGISTER_INFO(XMM4),
    REGISTER_INFO(XMM5),
    REGISTER_INFO(XMM6),
    REGISTER_INFO(XMM7),
    REGISTER_INFO(XMM8),
    REGISTER_INFO(XMM9),
    REGISTER_INFO(XMM10),
    REGISTER_INFO(XMM11),
    REGISTER_INFO(XMM12),
    REGISTER_INFO(XMM13),
    REGISTER_INFO(XMM14),
    REGISTER_INFO(XMM15),
};

const RegisterInfo *arch_get_register_info(Register reg)
{
    return ri[static_cast<uint16_t>(reg)];
}

const RegisterInfo *arch_get_register_info(RegisterType type, int nr)
{
    /* TODO lookup table */
    for (unsigned int i = 0; i < ARRAY_SIZE(ri); i++) {
        const RegisterInfo *r = ri[i];

        if (!r || r->parent != Register::None)
            continue;
        if (r->type != type)
            continue;
        if (r->nr == nr)
            return r;
    }

    return nullptr;
}

} /* namespace drob */
