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
#include <cstring>

#include "../arch.hpp"
#include "../SuperBlock.hpp"
#include "../BinaryPool.hpp"
#include "Converter.hpp"
#include "Encoder.hpp"
#include "x86.hpp"

namespace drob {

/*
 * Especially FLAGs can never be encoded and will bail out.
 */
static uint8_t encodeReg(Register reg)
{
    switch (reg) {
    case Register::AL:
    case Register::AX:
    case Register::EAX:
    case Register::RAX:
    case Register::XMM0:
        return 0;
    case Register::CL:
    case Register::CX:
    case Register::ECX:
    case Register::RCX:
    case Register::XMM1:
        return 1;
    case Register::DL:
    case Register::DX:
    case Register::EDX:
    case Register::RDX:
    case Register::XMM2:
        return 2;
    case Register::BL:
    case Register::BX:
    case Register::EBX:
    case Register::RBX:
    case Register::XMM3:
        return 3;
    case Register::AH:
    case Register::SPL:
    case Register::SP:
    case Register::ESP:
    case Register::RSP:
    case Register::XMM4:
        return 4;
    case Register::CH:
    case Register::BPL:
    case Register::BP:
    case Register::EBP:
    case Register::RBP:
    case Register::XMM5:
        return 5;
    case Register::DH:
    case Register::SIL:
    case Register::SI:
    case Register::ESI:
    case Register::RSI:
    case Register::XMM6:
        return 6;
    case Register::BH:
    case Register::DIL:
    case Register::DI:
    case Register::EDI:
    case Register::RDI:
    case Register::XMM7:
        return 7;
    case Register::R8B:
    case Register::R8W:
    case Register::R8D:
    case Register::R8:
    case Register::XMM8:
        return 8;
    case Register::R9B:
    case Register::R9W:
    case Register::R9D:
    case Register::R9:
    case Register::XMM9:
        return 9;
    case Register::R10B:
    case Register::R10W:
    case Register::R10D:
    case Register::R10:
    case Register::XMM10:
        return 10;
    case Register::R11B:
    case Register::R11W:
    case Register::R11D:
    case Register::R11:
    case Register::XMM11:
        return 11;
    case Register::R12B:
    case Register::R12W:
    case Register::R12D:
    case Register::R12:
    case Register::XMM12:
        return 12;
    case Register::R13B:
    case Register::R13W:
    case Register::R13D:
    case Register::R13:
    case Register::XMM13:
        return 13;
    case Register::R14B:
    case Register::R14W:
    case Register::R14D:
    case Register::R14:
    case Register::XMM14:
        return 14;
    case Register::R15B:
    case Register::R15W:
    case Register::R15D:
    case Register::R15:
    case Register::XMM15:
        return 15;
    default:
        drob_assert_not_reached();
    }
}

/*
 * Only selected (initial) 32bit registers can be used for register
 * indirect (SIB) addressing.
 */
static uint8_t encodeAddrReg(Register reg, bool *is32)
{
    switch (reg) {
    case Register::EAX:
        *is32 = true;
        /* FALL THROUGH */
    case Register::RAX:
        return 0;
    case Register::ECX:
        *is32 = true;
        /* FALL THROUGH */
    case Register::RCX:
        return 1;
    case Register::EDX:
        *is32 = true;
        /* FALL THROUGH */
    case Register::RDX:
        return 2;
    case Register::EBX:
        *is32 = true;
        /* FALL THROUGH */
    case Register::RBX:
        return 3;
    case Register::ESP:
        *is32 = true;
        /* FALL THROUGH */
    case Register::RSP:
        return 4;
    case Register::EBP:
        *is32 = true;
        /* FALL THROUGH */
    case Register::RBP:
        return 5;
    case Register::ESI:
        *is32 = true;
        /* FALL THROUGH */
    case Register::RSI:
        return 6;
    case Register::EDI:
        *is32 = true;
        /* FALL THROUGH */
    case Register::RDI:
        return 7;
    case Register::R8:
        return 8;
    case Register::R9:
        return 9;
    case Register::R10:
        return 10;
    case Register::R11:
        return 11;
    case Register::R12:
        return 12;
    case Register::R13:
        return 13;
    case Register::R14:
        return 14;
    case Register::R15:
        return 15;
    default:
        drob_assert_not_reached();
    }
}

typedef struct REX {
    REX() : val(0x40) {}
    bool isRequired(void)
    {
        return val != 0x40;
    }
    union {
        struct {
            uint8_t b : 1;
            uint8_t x : 1;
            uint8_t r : 1;
            uint8_t w : 1;
            uint8_t reserved : 4;
        } __attribute__ ((__packed__));
        uint8_t val;
    };
} REX;

typedef struct SIB {
    SIB() : required(false), val(0) {}
    bool isRequired(void)
    {
        return required;
    }

    bool required;
    union {
        struct {
            uint8_t base : 3;
            uint8_t index : 3;
            uint8_t scale : 2;
        } __attribute__ ((__packed__));
        uint8_t val;
    };
} SIB;

typedef struct ModRM {
    ModRM() : val(0) {}
    union {
        struct {
            uint8_t rm : 3;
            uint8_t reg : 3;
            uint8_t mod : 2;
        } __attribute__ ((__packed__));
        uint8_t val;
    };
} ModRM;

typedef struct Disp {
    Disp() : required(false), isDisp32(false), disp32(0) {}
    bool isRequired(void)
    {
        return required;
    }

    bool required;
    bool isDisp32;
    union {
        int8_t disp8;
        int32_t disp32;
    };
} Disp;

typedef unsigned int EncFlags;
#define ENC_FLAG_NONE 0
#define ENC_FLAG_REXW 1
#define ENC_FLAG_66 2
#define ENC_FLAG_F2 4

typedef class ModRMEncoding {
public:
    ModRMEncoding(const uint8_t *oc, uint8_t ocLen, Register reg,
              Register rm, EncFlags flags);
    ModRMEncoding(const uint8_t *oc, uint8_t ocLen, uint8_t reg,
              Register rm, EncFlags flags);
    ModRMEncoding(const uint8_t *oc, uint8_t ocLen, uint8_t reg,
              Register rm, uint64_t imm, uint8_t immLen, EncFlags flags);
    ModRMEncoding(const uint8_t *oc, uint8_t ocLen, Register reg,
              StaticMemPtr rm, EncFlags flags, uint64_t addr);
    ModRMEncoding(const uint8_t *oc, uint8_t ocLen, uint8_t reg,
              StaticMemPtr rm, EncFlags flags, uint64_t addr);
    ModRMEncoding(const uint8_t *oc, uint8_t ocLen, uint8_t reg,
              StaticMemPtr rm, uint64_t imm, uint8_t immLen, EncFlags flags,
              uint64_t addr);
    int write(uint8_t *data);
private:
    void encodeMemoryDirect(StaticMemPtr rm, uint64_t addr);
    void encodeMemoryIndirect(StaticMemPtr rm);

    /* the opcode */
    const uint8_t *oc;
    /* the length of the opcode */
    uint8_t ocLen;

    /* the immediate */
    uint64_t imm{0};
    /* the length of the immediate */
    uint8_t immLen{0};

    /* do we need a 32bit address overwrite? (0x67) */
    bool is32bit{false};
    /* Operand size overwrite (0x66) */
    bool p66{false};
    /* REPNE prefix (0xf2) */
    bool pf2{false};

    REX rex;
    ModRM modrm;
    SIB sib;
    Disp disp;
} ModRMEncoding;

ModRMEncoding::ModRMEncoding(const uint8_t *oc, uint8_t ocLen, Register reg,
                 Register rm, EncFlags flags) :
    oc(oc), ocLen(ocLen), p66(!!(flags & ENC_FLAG_66)),
    pf2(!!(flags & ENC_FLAG_F2))
{
    const uint8_t r = encodeReg(reg);
    const uint8_t b = encodeReg(rm);

    rex.w = !!(flags & ENC_FLAG_REXW);
    /* encode the reg register */
    rex.r = !!(r & 0x08);
    modrm.reg = r & 0x7;
    /* encode the rm register */
    rex.b = !!(b & 0x08);
    modrm.rm = b & 0x7;
    modrm.mod = 3;
}

ModRMEncoding::ModRMEncoding(const uint8_t *oc, uint8_t ocLen, uint8_t reg,
                 Register rm, EncFlags flags) :
    oc(oc), ocLen(ocLen), p66(!!(flags & ENC_FLAG_66)),
    pf2(!!(flags & ENC_FLAG_F2))
{
    const uint8_t b = encodeReg(rm);

    rex.w = !!(flags & ENC_FLAG_REXW);
    /* encode the reg register */
    rex.r = !!(reg & 0x08);
    modrm.reg = reg & 0x7;
    /* encode the rm register */
    rex.b = !!(b & 0x08);
    modrm.rm = b & 0x7;
    modrm.mod = 3;
}

ModRMEncoding::ModRMEncoding(const uint8_t *oc, uint8_t ocLen, uint8_t reg,
                 Register rm, uint64_t imm, uint8_t immLen, EncFlags flags) :
    oc(oc), ocLen(ocLen), imm(imm), immLen(immLen), p66(!!(flags & ENC_FLAG_66)),
    pf2(!!(flags & ENC_FLAG_F2))
{
    const uint8_t b = encodeReg(rm);

    rex.w = !!(flags & ENC_FLAG_REXW);
    /* encode the reg register */
    rex.r = !!(reg & 0x08);
    modrm.reg = reg & 0x7;
    /* encode the rm register */
    rex.b = !!(b & 0x08);
    modrm.rm = b & 0x7;
    modrm.mod = 3;
}

ModRMEncoding::ModRMEncoding(const uint8_t *oc, uint8_t ocLen, Register reg,
                 StaticMemPtr rm, EncFlags flags, uint64_t addr) :
    oc(oc), ocLen(ocLen), p66(!!(flags & ENC_FLAG_66)),
    pf2(!!(flags & ENC_FLAG_F2))
{
    uint8_t r = encodeReg(reg);

    rex.w = !!(flags & ENC_FLAG_REXW);
    /* encode the reg register */
    rex.r = !!(r & 0x08);
    modrm.reg = r & 0x7;

    /* encode the rm memory operand */
    if (rm.type == MemPtrType::Direct) {
        encodeMemoryDirect(rm, addr);
    } else {
        encodeMemoryIndirect(rm);
    }
}

ModRMEncoding::ModRMEncoding(const uint8_t *oc, uint8_t ocLen, uint8_t reg,
                 StaticMemPtr rm, EncFlags flags, uint64_t addr) :
    oc(oc), ocLen(ocLen), p66(!!(flags & ENC_FLAG_66)),
    pf2(!!(flags & ENC_FLAG_F2))
{
    rex.w = !!(flags & ENC_FLAG_REXW);
    /* encode the reg register */
    rex.r = !!(reg & 0x08);
    modrm.reg = reg & 0x7;

    /* encode the rm memory operand */
    if (rm.type == MemPtrType::Direct) {
        encodeMemoryDirect(rm, addr);
    } else {
        encodeMemoryIndirect(rm);
    }
}

ModRMEncoding::ModRMEncoding(const uint8_t *oc, uint8_t ocLen, uint8_t reg,
                 StaticMemPtr rm, uint64_t imm, uint8_t immLen,
                 EncFlags flags, uint64_t addr) :
    oc(oc), ocLen(ocLen), imm(imm), immLen(immLen), p66(!!(flags & ENC_FLAG_66)),
    pf2(!!(flags & ENC_FLAG_F2))
{
    rex.w = !!(flags & ENC_FLAG_REXW);
    /* encode the reg register */
    rex.r = !!(reg & 0x08);
    modrm.reg = reg & 0x7;

    /* encode the rm memory operand */
    if (rm.type == MemPtrType::Direct) {
        encodeMemoryDirect(rm, addr);
    } else {
        encodeMemoryIndirect(rm);
    }
}

int ModRMEncoding::write(uint8_t *data)
{
    int idx = 0;

    if (p66) {
        data[idx++] = 0x66;
    }
    if (is32bit) {
        data[idx++] = 0x67;
    }
    if (pf2) {
        data[idx++] = 0xf2;
    }
    if (rex.isRequired()) {
        data[idx++] = rex.val;
    }
    for (int i = 0; i < ocLen; i++) {
        data[idx++] = oc[i];
    }
    data[idx++] = modrm.val;

    if (sib.isRequired()) {
        data[idx++] = sib.val;
    }
    if (disp.isRequired()) {
        if (disp.isDisp32) {
            *((uint32_t *)&data[idx]) = disp.disp32;
            idx += 4;
        } else {
            data[idx++] = disp.disp8;
        }
    }
    switch (immLen) {
    case 1:
        *((uint8_t *)&data[idx]) = imm;
        idx += 1;
        break;
    case 2:
        *((uint16_t *)&data[idx]) = imm;
        idx += 2;
        break;
    case 4:
        *((uint32_t *)&data[idx]) = imm;
        idx += 4;
        break;
    default:
        drob_assert(!immLen);
        break;
    }
    return idx;
}

void ModRMEncoding::encodeMemoryDirect(StaticMemPtr rm, uint64_t addr)
{
    /* Can address via 32bit */
    if ((int32_t)rm.addr.val == (int64_t)rm.addr.val) {
        /* Disp32 only */
        modrm.mod = 0;
        modrm.rm = 4;
        sib.required = true;
        sib.base = 5;
        sib.index = 4;
        disp.required = true;
        disp.isDisp32 = true;
        disp.disp32 = rm.addr.val;
        return;
    }

    /* Can reach via RIP+rel32 - calculate ilen */
    int ilen = ocLen + 5 + immLen; /* oc + ModRM byte + 4byte rel32 */
    if (p66) {
        ilen += 1;
    }
    if (is32bit) {
        ilen += 1;
    }
    if (pf2) {
        ilen += 1;
    }
    if (rex.isRequired()) {
        ilen += 1;
    }
    drob_assert(!sib.isRequired());

    int64_t rel = rm.addr.val - (addr + ilen);
    if (is_rel32(rel)) {
        modrm.mod = 0;
        modrm.rm = 5;
        disp.required = true;
        disp.isDisp32 = true;
        disp.disp32 = rel;
        return;
    }

    /*
     * TODO: A pass (e.g. MemoryOperandOptimizationPass or similar) should
     * detect this scenario, find a dead register (or free one up), load the
     * value and modify this address accordingly. Once we reached this point,
     * it's too late.
     */
    drob_error("Can't reach memory operand");
}

void ModRMEncoding::encodeMemoryIndirect(StaticMemPtr rm)
{
    uint8_t r;

    switch (rm.sib.base) {
    /* (E|R)BP+R13 need explicit disp due to RIP+disp32 */
    case Register::EBP:
        is32bit = true;
        /* FALL THROUGH */
    case Register::RBP:
    case Register::R13:
        disp.required = true;
        break;
    /* The stack pointer can never be an index register */
    case Register::ESP:
    case Register::RSP:
        drob_assert_not_reached();
    default:
        break;
    }

    /* Convert the SIB scale although we might not need it */
    switch (rm.sib.scale) {
    case 0:
        /* no scale */
    case 1:
        sib.scale = 0;
        break;
    case 2:
        sib.scale = 1;
        break;
    case 4:
        sib.scale = 2;
        break;
    case 8:
        sib.scale = 3;
        break;
    default:
        drob_assert_not_reached();
    }

    /* disp only has to be encoded as RawMemPtrType::Direct */
    drob_assert(rm.sib.base != Register::None ||
            rm.sib.index != Register::None);

    if (rm.sib.index == Register::None) {
        /* try to compress the displacement */
        if (rm.sib.disp.val) {
            disp.required = true;
            if ((int8_t)rm.sib.disp.val == rm.sib.disp.val) {
                disp.disp8 = rm.sib.disp.val;
            } else {
                disp.isDisp32 = true;
                disp.disp32 = rm.sib.disp.val;
            }
        }
        if (disp.required) {
            if (disp.isDisp32) {
                modrm.mod = 2;
            } else {
                modrm.mod = 1;
            }
        } else {
            modrm.mod = 0;
        }
        r = encodeAddrReg(rm.sib.base, &is32bit);
        modrm.rm = r & 0x07;
        rex.b = !!(r & 0x08);
    } else if (rm.sib.base == Register::None) {
        /* TODO: we could optimize scale=1, index=X and encode
         * it as base only. */
        modrm.mod = 0;
        modrm.rm = 4;
        sib.required = true;
        sib.base = 5;
        r = encodeAddrReg(rm.sib.index, &is32bit);
        sib.index = r & 0x07;
        rex.x = !!(r & 0x08);
        /* we have to encode a 32bit disp */
        disp.required = true;
        disp.isDisp32 = true;
        disp.disp32 = rm.sib.disp.val;
    } else {
        /* try to compress the displacement */
        if (rm.sib.disp.val) {
            disp.required = true;
            if ((int8_t)rm.sib.disp.val == rm.sib.disp.val) {
                disp.disp8 = rm.sib.disp.val;
            } else {
                disp.isDisp32 = true;
                disp.disp32 = rm.sib.disp.val;
            }
        }
        if (disp.required) {
            if (disp.isDisp32) {
                modrm.mod = 2;
            } else {
                modrm.mod = 1;
            }
        } else {
            modrm.mod = 0;
        }
        modrm.rm = 4;

        /* we definetly need the SIB */
        sib.required = true;
        r = encodeAddrReg(rm.sib.base, &is32bit);
        sib.base = r & 0x07;
        rex.b = !!(r & 0x08);

        bool needs32bit = false;
        r = encodeAddrReg(rm.sib.index, &needs32bit);
        sib.index = r & 0x07;
        rex.x = !!(r & 0x08);
        drob_assert(is32bit == needs32bit);
    }
}

static inline int write_modrm_m_reg(uint8_t oc, const ExplicitStaticOperands &explOperands,
                                    EncFlags flags, uint8_t *buf, uint64_t addr)
{
    ModRMEncoding ope(&oc, 1, explOperands.op[1].reg, explOperands.op[0].mem,
                      flags, addr);
    return ope.write(buf);
}

static inline int write_modrm_m(uint8_t oc, uint8_t oce,
                                const ExplicitStaticOperands &explOperands, EncFlags flags,
                                uint8_t *buf, uint64_t addr)
{
    ModRMEncoding ope(&oc, 1, oce, explOperands.op[0].mem, flags, addr);
    return ope.write(buf);
}

static inline int write_modrm_r(uint8_t oc, uint8_t oce,
                                const ExplicitStaticOperands &explOperands, EncFlags flags,
                                uint8_t *buf)
{
    ModRMEncoding ope(&oc, 1, oce, explOperands.op[0].reg, flags);
    return ope.write(buf);
}

static inline int write_modrm_m_reg(uint8_t oc1, uint8_t oc2,
                                    const ExplicitStaticOperands &explOperands,
                                    EncFlags flags, uint8_t *buf, uint64_t addr)
{
    const uint8_t oc[2] = { oc1, oc2 };
    ModRMEncoding ope(oc, sizeof(oc), explOperands.op[1].reg,
                      explOperands.op[0].mem, flags, addr);
    return ope.write(buf);
}

static inline int write_modrm_reg_m(uint8_t oc1, uint8_t oc2,
                                    const ExplicitStaticOperands &explOperands,
                                    EncFlags flags, uint8_t *buf, uint64_t addr)
{
    const uint8_t oc[2] = { oc1, oc2 };
    ModRMEncoding ope(oc, sizeof(oc), explOperands.op[0].reg,
                      explOperands.op[1].mem, flags, addr);
    return ope.write(buf);
}

static inline int write_modrm_reg_m(uint8_t oc, const ExplicitStaticOperands &explOperands,
                                    EncFlags flags, uint8_t *buf, uint64_t addr)
{
    ModRMEncoding ope(&oc, 1, explOperands.op[0].reg, explOperands.op[1].mem,
                      flags, addr);
    return ope.write(buf);
}

static inline int write_modrm_r_reg(uint8_t oc, const ExplicitStaticOperands &explOperands,
                                    EncFlags flags, uint8_t *buf)
{
    ModRMEncoding ope(&oc, 1, explOperands.op[1].reg, explOperands.op[0].reg,
                      flags);
    return ope.write(buf);
}

static inline int write_modrm_r_reg(uint8_t oc1, uint8_t oc2,
                                    const ExplicitStaticOperands &explOperands,
                                    EncFlags flags, uint8_t *buf)
{
    const uint8_t oc[2] = { oc1, oc2 };
    ModRMEncoding ope(oc, sizeof(oc), explOperands.op[1].reg,
                      explOperands.op[0].reg, flags);
    return ope.write(buf);
}

static inline int write_modrm_reg_r(uint8_t oc1, uint8_t oc2,
                                    const ExplicitStaticOperands &explOperands,
                                    EncFlags flags, uint8_t *buf)
{
    const uint8_t oc[2] = { oc1, oc2 };
    ModRMEncoding ope(oc, sizeof(oc), explOperands.op[0].reg,
                      explOperands.op[1].reg, flags);
    return ope.write(buf);
}

static inline int write_modrm_reg_r(uint8_t oc, const ExplicitStaticOperands &explOperands,
                                    EncFlags flags, uint8_t *buf)
{
    ModRMEncoding ope(&oc, 1, explOperands.op[0].reg, explOperands.op[1].reg,
                      flags);
    return ope.write(buf);
}

static inline int write_modrm_m_imm8(uint8_t oc, uint8_t oce,
                                     const ExplicitStaticOperands &explOperands,
                                     EncFlags flags, uint8_t *buf,
                                     uint64_t addr)
{
    ModRMEncoding ope(&oc, 1, oce, explOperands.op[0].mem, explOperands.op[1].imm.val,
                      1, flags, addr);
    return ope.write(buf);
}

static inline int write_modrm_m_imm16(uint8_t oc, uint8_t oce,
                                      const ExplicitStaticOperands &explOperands,
                                      EncFlags flags, uint8_t *buf,
                                      uint64_t addr)
{
    ModRMEncoding ope(&oc, 1, oce, explOperands.op[0].mem, explOperands.op[1].imm.val,
                      2, flags, addr);
    return ope.write(buf);
}

static inline int write_modrm_m_imm32(uint8_t oc, uint8_t oce,
                                      const ExplicitStaticOperands &explOperands,
                                      EncFlags flags, uint8_t *buf,
                                      uint64_t addr)
{
    ModRMEncoding ope(&oc, 1, oce, explOperands.op[0].mem, explOperands.op[1].imm.val,
                      4, flags, addr);
    return ope.write(buf);
}

static inline int write_modrm_r_imm8(uint8_t oc, uint8_t oce,
                                     const ExplicitStaticOperands &explOperands,
                                     EncFlags flags, uint8_t *buf)
{
    ModRMEncoding ope(&oc, 1, oce, explOperands.op[0].reg, explOperands.op[1].imm.val,
                      1, flags);
    return ope.write(buf);
}

static inline int write_modrm_r_imm16(uint8_t oc, uint8_t oce,
                                      const ExplicitStaticOperands &explOperands,
                                      EncFlags flags, uint8_t *buf)
{
    ModRMEncoding ope(&oc, 1, oce, explOperands.op[0].reg, explOperands.op[1].imm.val,
                      2, flags);
    return ope.write(buf);
}

static inline int write_modrm_r_imm32(uint8_t oc, uint8_t reg,
                                      const ExplicitStaticOperands &explOperands,
                                      EncFlags flags, uint8_t *buf)
{
    ModRMEncoding ope(&oc, 1, reg, explOperands.op[0].reg, explOperands.op[1].imm.val,
                      4, flags);
    return ope.write(buf);
}

/* Encode the special "oc + rw/rd" format */
static inline int write_reg(uint8_t oc, const ExplicitStaticOperands &explOperands,
                            EncFlags flags, uint8_t *buf)
{
    uint8_t reg = encodeReg(explOperands.op[0].reg);
    int idx = 0;
    REX rex;

    if (flags & ENC_FLAG_REXW) {
        rex.w = 1;
    }
    rex.b = !!(reg & 0x08);
    if (flags & ENC_FLAG_66) {
        buf[idx++] = 0x66;
    }
    if (flags & ENC_FLAG_F2) {
        buf[idx++] = 0xF2;
    }
    if (rex.isRequired()) {
        buf[idx++] = rex.val;
    }
    buf[idx++] = oc | (reg & 0x7);
    return idx;
}

static inline int write_reg_imm64(uint8_t oc, const ExplicitStaticOperands &explOperands,
                                  EncFlags flags, uint8_t *buf)
{
    int idx = write_reg(oc, explOperands, flags, buf);

    *((uint64_t *)&buf[idx]) = explOperands.op[1].imm.val;
    return idx + 8;
}

static inline int write_reg_imm32(uint8_t oc, const ExplicitStaticOperands &explOperands,
                                  EncFlags flags, uint8_t *buf)
{
    int idx = write_reg(oc, explOperands, flags, buf);

    *((uint32_t *)&buf[idx]) = explOperands.op[1].imm.val;
    return idx + 4;
}

static inline int write_imm8(uint8_t oc, uint8_t imm, EncFlags flags,
                             uint8_t *buf)
{
    REX rex;
    int idx = 0;

    if (flags & ENC_FLAG_REXW) {
        rex.w = 1;
    }
    if (flags & ENC_FLAG_66) {
        buf[idx++] = 0x66;
    }
    if (flags & ENC_FLAG_F2) {
        buf[idx++] = 0xF2;
    }
    if (rex.isRequired()) {
        buf[idx++] = rex.val;
    }
    buf[idx++] = oc;
    *((uint8_t *)&buf[idx]) = imm;
    return idx + 1;
}

static inline int write_imm16(uint8_t oc, uint16_t imm, EncFlags flags,
                              uint8_t *buf)
{
    REX rex;
    int idx = 0;

    if (flags & ENC_FLAG_REXW) {
        rex.w = 1;
    }
    if (flags & ENC_FLAG_66) {
        buf[idx++] = 0x66;
    }
    if (flags & ENC_FLAG_F2) {
        buf[idx++] = 0xF2;
    }
    if (rex.isRequired()) {
        buf[idx++] = rex.val;
    }
    buf[idx++] = oc;
    *((uint16_t *)&buf[idx]) = imm;
    return idx + 2;
}

static inline int write_imm32(uint8_t oc, int32_t imm, EncFlags flags,
                              uint8_t *buf)
{
    REX rex;
    int idx = 0;

    if (flags & ENC_FLAG_REXW) {
        rex.w = 1;
    }
    if (flags & ENC_FLAG_66) {
        buf[idx++] = 0x66;
    }
    if (flags & ENC_FLAG_F2) {
        buf[idx++] = 0xF2;
    }
    if (rex.isRequired()) {
        buf[idx++] = rex.val;
    }
    buf[idx++] = oc;
    *((uint32_t *)&buf[idx]) = imm;
    return idx + 4;
}

DEF_ENCODE_FN(add)
{
    switch (opcode) {
    case Opcode::ADD8mr:
        return write_modrm_m_reg(0x00, explOperands, ENC_FLAG_NONE, buf, addr);
    case Opcode::ADD16mr:
        return write_modrm_m_reg(0x01, explOperands, ENC_FLAG_66, buf, addr);
    case Opcode::ADD32mr:
        return write_modrm_m_reg(0x01, explOperands, ENC_FLAG_NONE, buf, addr);
    case Opcode::ADD64mr:
        return write_modrm_m_reg(0x01, explOperands, ENC_FLAG_REXW, buf, addr);

    case Opcode::ADD8rr:
        return write_modrm_r_reg(0x00, explOperands, ENC_FLAG_NONE, buf);
    case Opcode::ADD16rr:
        return write_modrm_r_reg(0x01, explOperands, ENC_FLAG_66, buf);
    case Opcode::ADD32rr:
        return write_modrm_r_reg(0x01, explOperands, ENC_FLAG_NONE, buf);
    case Opcode::ADD64rr:
        return write_modrm_r_reg(0x01, explOperands, ENC_FLAG_REXW, buf);

    case Opcode::ADD8rm:
        return write_modrm_reg_m(0x02, explOperands, ENC_FLAG_NONE, buf, addr);
    case Opcode::ADD16rm:
        return write_modrm_reg_m(0x03, explOperands, ENC_FLAG_66, buf, addr);
    case Opcode::ADD32rm:
        return write_modrm_reg_m(0x03, explOperands, ENC_FLAG_NONE, buf, addr);
    case Opcode::ADD64rm:
        return write_modrm_reg_m(0x03, explOperands, ENC_FLAG_REXW, buf, addr);

    case Opcode::ADD8mi:
        return write_modrm_m_imm8(0x80, 0, explOperands, ENC_FLAG_NONE, buf,
                                   addr);
    case Opcode::ADD16mi:
        if (is_simm8(explOperands.op[1].imm.val)) {
            return write_modrm_m_imm8(0x83, 0, explOperands, ENC_FLAG_66, buf,
                                      addr);
        }
        return write_modrm_m_imm16(0x81, 0, explOperands, ENC_FLAG_66, buf,
                                   addr);
    case Opcode::ADD32mi:
        if (is_simm8(explOperands.op[1].imm.val)) {
            return write_modrm_m_imm8(0x83, 0, explOperands, ENC_FLAG_NONE, buf,
                                      addr);
        }
        return write_modrm_m_imm32(0x81, 0, explOperands, ENC_FLAG_NONE, buf,
                                   addr);
    case Opcode::ADD64mi:
        if (is_simm8(explOperands.op[1].imm.val)) {
            return write_modrm_m_imm8(0x83, 0, explOperands, ENC_FLAG_REXW, buf,
                                      addr);
        }
        return write_modrm_m_imm32(0x81, 0, explOperands, ENC_FLAG_REXW, buf,
                                   addr);

    case Opcode::ADD8ri:
        if (explOperands.op[0].reg == Register::AL) {
            return write_imm8(0x04, explOperands.op[1].imm.val, ENC_FLAG_NONE, buf);
        }
        return write_modrm_r_imm8(0x80, 0, explOperands, ENC_FLAG_NONE, buf);
    case Opcode::ADD16ri:
        if (is_simm8(explOperands.op[1].imm.val)) {
            return write_modrm_r_imm8(0x83, 0, explOperands, ENC_FLAG_66, buf);
        } else if (explOperands.op[0].reg == Register::AX) {
            return write_imm16(0x05, explOperands.op[1].imm.val, ENC_FLAG_66, buf);
        }
        return write_modrm_r_imm16(0x81, 0, explOperands, ENC_FLAG_66, buf);
    case Opcode::ADD32ri:
        if (is_simm8(explOperands.op[1].imm.val)) {
            return write_modrm_r_imm8(0x83, 0, explOperands, ENC_FLAG_NONE, buf);
        } else if (explOperands.op[0].reg == Register::EAX) {
            return write_imm32(0x05, explOperands.op[1].imm.val, ENC_FLAG_NONE, buf);
        }
        return write_modrm_r_imm32(0x81, 0, explOperands, ENC_FLAG_NONE, buf);
    case Opcode::ADD64ri:
        if (is_simm8(explOperands.op[1].imm.val)) {
            return write_modrm_r_imm8(0x83, 0, explOperands, ENC_FLAG_REXW, buf);
        } else if (explOperands.op[0].reg == Register::RAX) {
            return write_imm32(0x05, explOperands.op[1].imm.val, ENC_FLAG_REXW, buf);
        }
        return write_modrm_r_imm32(0x81, 0, explOperands, ENC_FLAG_REXW, buf);
    default:
        drob_assert_not_reached();
    }
}

DEF_ENCODE_FN(addpd)
{
    switch (opcode) {
    case Opcode::ADDPDrr:
        return write_modrm_reg_r(0x0f, 0x58, explOperands, ENC_FLAG_66, buf);
    case Opcode::ADDPDrm:
        return write_modrm_reg_m(0x0f, 0x58, explOperands, ENC_FLAG_66, buf,
                                 addr);
    default:
        drob_assert_not_reached();
    }
}

DEF_ENCODE_FN(addsd)
{
    switch (opcode) {
    case Opcode::ADDSDrr:
        return write_modrm_reg_r(0x0f, 0x58, explOperands, ENC_FLAG_F2, buf);
    case Opcode::ADDSDrm:
        return write_modrm_reg_m(0x0f, 0x58, explOperands, ENC_FLAG_F2, buf,
                                 addr);
    default:
        drob_assert_not_reached();
    }
}

DEF_ENCODE_FN(call)
{
    switch (opcode) {
    case Opcode::CALLr:
        return write_modrm_r(0xff, 0x03, explOperands, ENC_FLAG_NONE, buf);
    case Opcode::CALLm:
        return write_modrm_m(0xff, 0x03, explOperands, ENC_FLAG_NONE, buf, addr);
    default:
        /* We can always predict the real target for CALL */
        drob_assert_not_reached();
    };
}

DEF_ENCODE_FN(cmp)
{
    switch (opcode) {
    case Opcode::CMP8mr:
        return write_modrm_m_reg(0x38, explOperands, ENC_FLAG_NONE, buf, addr);
    case Opcode::CMP8mi:
        return write_modrm_m_imm8(0x80, 7, explOperands, ENC_FLAG_NONE, buf,
                                  addr);
    case Opcode::CMP8rm:
        return write_modrm_reg_m(0x3a, explOperands, ENC_FLAG_NONE, buf, addr);
    case Opcode::CMP8rr:
        return write_modrm_r_reg(0x38, explOperands, ENC_FLAG_NONE, buf);
    case Opcode::CMP8ri:
        if (explOperands.op[0].reg == Register::AL) {
            return write_imm8(0x3c, explOperands.op[1].imm.val, ENC_FLAG_NONE, buf);
        }
        return write_modrm_r_imm8(0x80, 7, explOperands, ENC_FLAG_NONE, buf);
    case Opcode::CMP16mr:
        return write_modrm_m_reg(0x39, explOperands, ENC_FLAG_66, buf, addr);
    case Opcode::CMP16mi:
        if (is_simm8(explOperands.op[1].imm.val)) {
            return write_modrm_m_imm8(0x83, 7, explOperands, ENC_FLAG_66, buf,
                                      addr);
        }
        return write_modrm_m_imm16(0x81, 7, explOperands, ENC_FLAG_66, buf, addr);
    case Opcode::CMP16rm:
        return write_modrm_reg_m(0x3b, explOperands, ENC_FLAG_66, buf, addr);
    case Opcode::CMP16rr:
        return write_modrm_r_reg(0x39, explOperands, ENC_FLAG_66, buf);
    case Opcode::CMP16ri:
        if (is_simm8(explOperands.op[1].imm.val)) {
            return write_modrm_r_imm8(0x83, 7, explOperands, ENC_FLAG_66, buf);
        } else if (explOperands.op[0].reg == Register::EAX) {
            return write_imm16(0x3d, explOperands.op[1].imm.val, ENC_FLAG_66, buf);
        }
        return write_modrm_r_imm16(0x81, 7, explOperands, ENC_FLAG_66, buf);
    case Opcode::CMP32mr:
        return write_modrm_m_reg(0x39, explOperands, ENC_FLAG_NONE, buf, addr);
    case Opcode::CMP32mi:
        if (is_simm8(explOperands.op[1].imm.val)) {
            return write_modrm_m_imm8(0x83, 7, explOperands, ENC_FLAG_NONE, buf,
                                      addr);
        }
        return write_modrm_m_imm32(0x81, 7, explOperands, ENC_FLAG_NONE, buf,
                                   addr);
    case Opcode::CMP32rm:
        return write_modrm_reg_m(0x3b, explOperands, ENC_FLAG_NONE, buf, addr);
    case Opcode::CMP32rr:
        return write_modrm_r_reg(0x39, explOperands, ENC_FLAG_NONE, buf);
    case Opcode::CMP32ri:
        if (is_simm8(explOperands.op[1].imm.val)) {
            return write_modrm_r_imm8(0x83, 7, explOperands, ENC_FLAG_NONE, buf);
        } else if (explOperands.op[0].reg == Register::EAX) {
            return write_imm32(0x3d, explOperands.op[1].imm.val, ENC_FLAG_NONE, buf);
        }
        return write_modrm_r_imm32(0x81, 7, explOperands, ENC_FLAG_NONE, buf);
    case Opcode::CMP64mr:
        return write_modrm_m_reg(0x39, explOperands, ENC_FLAG_REXW, buf, addr);
    case Opcode::CMP64mi:
        if (is_simm8(explOperands.op[1].imm.val)) {
            return write_modrm_m_imm8(0x83, 7, explOperands, ENC_FLAG_REXW, buf,
                                      addr);
        }
        return write_modrm_m_imm32(0x81, 7, explOperands, ENC_FLAG_REXW, buf,
                                   addr);
    case Opcode::CMP64rm:
        return write_modrm_reg_m(0x3b, explOperands, ENC_FLAG_REXW, buf, addr);
    case Opcode::CMP64rr:
        return write_modrm_r_reg(0x39, explOperands, ENC_FLAG_REXW, buf);
    case Opcode::CMP64ri:
        if (is_simm8(explOperands.op[1].imm.val)) {
            return write_modrm_r_imm8(0x83, 7, explOperands, ENC_FLAG_REXW, buf);
        } else if (explOperands.op[0].reg == Register::RAX) {
            return write_imm32(0x3d, explOperands.op[1].imm.val, ENC_FLAG_REXW, buf);
        }
        return write_modrm_r_imm32(0x81, 7, explOperands, ENC_FLAG_REXW, buf);
    default:
        drob_assert_not_reached();
    }
}

DEF_ENCODE_FN(jcc)
{
    (void)opcode;
    (void)explOperands;
    (void)buf;
    (void)addr;
    /* We can always predict the real target */
    drob_assert_not_reached();
}

DEF_ENCODE_FN(jmp)
{
    switch (opcode) {
    case Opcode::JMPr:
        return write_modrm_r(0xff, 0x04, explOperands, ENC_FLAG_66, buf);
    case Opcode::JMPm:
        return write_modrm_m(0xff, 0x04, explOperands, ENC_FLAG_66, buf, addr);
    default:
        /* We can always predict the real target for JMP */
        drob_assert_not_reached();
    };
}

DEF_ENCODE_FN(lea)
{
    switch (opcode) {
    case Opcode::LEA16ra:
        return write_modrm_reg_m(0x8d, explOperands, ENC_FLAG_66, buf, addr);
    case Opcode::LEA32ra:
        return write_modrm_reg_m(0x8d, explOperands, ENC_FLAG_NONE, buf, addr);
    case Opcode::LEA64ra:
        return write_modrm_reg_m(0x8d, explOperands, ENC_FLAG_REXW, buf, addr);
    default:
        drob_assert_not_reached();
    }
}

DEF_ENCODE_FN(mov)
{
    (void)addr;
    switch (opcode) {
    case Opcode::MOV64mr:
        /* special encoding for moffs64 */
        if (explOperands.op[0].mem.type == MemPtrType::Direct &&
            explOperands.op[1].reg == Register::RAX &&
            !isDisp32(explOperands.op[0].mem.addr.val)) {
            int idx = 0;
            REX rex;

            rex.w = 1;
            buf[idx++] = rex.val;
            buf[idx++] = 0xa3;
            *((int64_t *)&buf[idx]) = explOperands.op[0].mem.addr.val;
            return idx + 8;
        }
        return write_modrm_m_reg(0x89, explOperands, ENC_FLAG_REXW, buf, addr);
    case Opcode::MOV64rr:
        return write_modrm_r_reg(0x89, explOperands, ENC_FLAG_REXW, buf);
    case Opcode::MOV64rm:
        /* special encoding for moffs64 */
        if (explOperands.op[1].mem.type == MemPtrType::Direct &&
            explOperands.op[0].reg == Register::RAX &&
            !isDisp32(explOperands.op[1].mem.addr.val)) {
            int idx = 0;
            REX rex;

            rex.w = 1;
            buf[idx++] = rex.val;
            buf[idx++] = 0xa1;
            *((int64_t *)&buf[idx]) = explOperands.op[1].mem.addr.val;
            return idx + 8;
        }
        return write_modrm_reg_m(0x8b, explOperands, ENC_FLAG_REXW, buf, addr);
    case Opcode::MOV64mi:
        return write_modrm_m_imm32(0xc7, 0, explOperands, ENC_FLAG_REXW, buf,
                                   addr);
    case Opcode::MOV64ri:
        if (is_simm32(explOperands.op[0].imm.val)) {
            return write_modrm_r_imm32(0xc7, 0, explOperands, ENC_FLAG_REXW, buf);
        }
        return write_reg_imm64(0xb8, explOperands, ENC_FLAG_REXW, buf);
    case Opcode::MOV32mr:
        return write_modrm_m_reg(0x89, explOperands, ENC_FLAG_NONE, buf, addr);
    case Opcode::MOV32rr:
        return write_modrm_r_reg(0x89, explOperands, ENC_FLAG_NONE, buf);
    case Opcode::MOV32rm:
        return write_modrm_reg_m(0x8b, explOperands, ENC_FLAG_NONE, buf, addr);
    case Opcode::MOV32mi:
        return write_modrm_m_imm32(0xc7, 0, explOperands, ENC_FLAG_NONE, buf,
                                   addr);
    case Opcode::MOV32ri:
        return write_reg_imm32(0xb8, explOperands, ENC_FLAG_NONE, buf);
    default:
        drob_assert_not_reached();
    }
}

DEF_ENCODE_FN(movapd)
{
    switch (opcode) {
    case Opcode::MOVAPDrm:
        return write_modrm_reg_m(0x0f, 0x28, explOperands, ENC_FLAG_66, buf,
                                 addr);
    case Opcode::MOVAPDrr:
        return write_modrm_reg_r(0x0f, 0x28, explOperands, ENC_FLAG_66, buf);
    case Opcode::MOVAPDmr:
        return write_modrm_m_reg(0x0f, 0x29, explOperands, ENC_FLAG_66, buf,
                                 addr);
    default:
        drob_assert_not_reached();
    }
}

DEF_ENCODE_FN(movsd)
{
    switch (opcode) {
    case Opcode::MOVSDrm:
        return write_modrm_reg_m(0x0f, 0x10, explOperands, ENC_FLAG_F2, buf,
                                 addr);
    case Opcode::MOVSDrr:
        return write_modrm_reg_r(0x0f, 0x10, explOperands, ENC_FLAG_F2, buf);
    case Opcode::MOVSDmr:
        return write_modrm_m_reg(0x0f, 0x11, explOperands, ENC_FLAG_F2, buf,
                                 addr);
    default:
        drob_assert_not_reached();
    }
}

DEF_ENCODE_FN(movupd)
{
    switch (opcode) {
    case Opcode::MOVUPDmr:
        return write_modrm_m_reg(0x0f, 0x11, explOperands, ENC_FLAG_66, buf,
                                 addr);
    case Opcode::MOVUPDrr:
        return write_modrm_reg_r(0x0f, 0x10, explOperands, ENC_FLAG_66, buf);
    case Opcode::MOVUPDrm:
        return write_modrm_reg_m(0x0f, 0x10, explOperands, ENC_FLAG_66, buf,
                                 addr);
    default:
        drob_assert_not_reached();
    }
}

DEF_ENCODE_FN(movups)
{
    switch (opcode) {
    case Opcode::MOVUPSmr:
        return write_modrm_m_reg(0x0f, 0x11, explOperands, ENC_FLAG_NONE, buf,
                                 addr);
    case Opcode::MOVUPSrr:
        return write_modrm_reg_r(0x0f, 0x10, explOperands, ENC_FLAG_NONE, buf);
    case Opcode::MOVUPSrm:
        return write_modrm_reg_m(0x0f, 0x10, explOperands, ENC_FLAG_NONE, buf,
                                 addr);
    default:
        drob_assert_not_reached();
    }
}

DEF_ENCODE_FN(mulpd)
{
    switch (opcode) {
    case Opcode::MULPDrr:
        return write_modrm_reg_r(0x0f, 0x59, explOperands, ENC_FLAG_66, buf);
    case Opcode::MULPDrm:
        return write_modrm_reg_m(0x0f, 0x59, explOperands, ENC_FLAG_66, buf,
                                 addr);
    default:
        drob_assert_not_reached();
    }
}

DEF_ENCODE_FN(mulsd)
{
    switch (opcode) {
    case Opcode::MULSDrr:
        return write_modrm_reg_r(0x0f, 0x59, explOperands, ENC_FLAG_F2, buf);
    case Opcode::MULSDrm:
        return write_modrm_reg_m(0x0f, 0x59, explOperands, ENC_FLAG_F2, buf,
                                 addr);
    default:
        drob_assert_not_reached();
    }
}

DEF_ENCODE_FN(pop)
{
    switch (opcode) {
    case Opcode::POP16m:
        return write_modrm_m(0x8f, 0, explOperands, ENC_FLAG_66, buf, addr);
    case Opcode::POP16r:
        return write_reg(0x58, explOperands, ENC_FLAG_66, buf);
    case Opcode::POP64m:
        return write_modrm_m(0x8f, 0, explOperands, ENC_FLAG_NONE, buf, addr);
    case Opcode::POP64r:
        return write_reg(0x58, explOperands, ENC_FLAG_NONE, buf);
    default:
        drob_assert_not_reached();
    }
}

DEF_ENCODE_FN(push)
{
    switch (opcode) {
    case Opcode::PUSH16m:
        return write_modrm_m(0xff, 6, explOperands, ENC_FLAG_66, buf, addr);
    case Opcode::PUSH16r:
        return write_reg(0x50, explOperands, ENC_FLAG_66, buf);
    case Opcode::PUSH16i:
        return write_imm16(0x68, explOperands.op[0].imm.val, ENC_FLAG_66, buf);
    case Opcode::PUSH64m:
        return write_modrm_m(0xff, 6, explOperands, ENC_FLAG_NONE, buf, addr);
    case Opcode::PUSH64r:
        return write_reg(0x50, explOperands, ENC_FLAG_NONE, buf);
    case Opcode::PUSH64i:
        if (is_simm8(explOperands.op[0].imm.val)) {
            return write_imm8(0x6a, explOperands.op[0].imm.val, ENC_FLAG_NONE, buf);
        }
        return write_imm32(0x68, explOperands.op[0].imm.val, ENC_FLAG_NONE, buf);
    default:
        drob_assert_not_reached();
    }
}

DEF_ENCODE_FN(pxor)
{
    switch (opcode) {
    case Opcode::PXOR128rr:
        return write_modrm_reg_r(0x0f, 0xef, explOperands, ENC_FLAG_66, buf);
    case Opcode::PXOR128rm:
        return write_modrm_reg_m(0x0f, 0xef, explOperands, ENC_FLAG_66, buf,
                                 addr);
    default:
        drob_assert_not_reached();
    }
}

DEF_ENCODE_FN(shl)
{
    switch (opcode) {
    case Opcode::SHL64m:
        return write_modrm_m(0xd3, 4, explOperands, ENC_FLAG_REXW, buf, addr);
    case Opcode::SHL64r:
        return write_modrm_r(0xd3, 4, explOperands, ENC_FLAG_REXW, buf);
    case Opcode::SHL64mi:
        if (explOperands.op[2].imm.val == 1) {
            return write_modrm_m(0xd1, 4, explOperands, ENC_FLAG_REXW, buf, addr);
        }
        return write_modrm_m_imm8(0xc1, 4, explOperands, ENC_FLAG_REXW, buf,
                                  addr);
    case Opcode::SHL64ri:
        if (explOperands.op[2].imm.val == 1) {
            return write_modrm_r(0xd1, 4, explOperands, ENC_FLAG_REXW, buf);
        }
        return write_modrm_r_imm8(0xc1, 4, explOperands, ENC_FLAG_REXW, buf);
    default:
        drob_assert_not_reached();
    }
}

DEF_ENCODE_FN(shr)
{
    switch (opcode) {
    case Opcode::SHR64m:
        return write_modrm_m(0xd3, 5, explOperands, ENC_FLAG_REXW, buf, addr);
    case Opcode::SHR64r:
        return write_modrm_r(0xd3, 5, explOperands, ENC_FLAG_REXW, buf);
    case Opcode::SHR64mi:
        if (explOperands.op[2].imm.val == 1) {
            return write_modrm_m(0xd1, 5, explOperands, ENC_FLAG_REXW, buf, addr);
        }
        return write_modrm_m_imm8(0xc1, 5, explOperands, ENC_FLAG_REXW, buf,
                                  addr);
    case Opcode::SHR64ri:
        if (explOperands.op[2].imm.val == 1) {
            return write_modrm_r(0xd1, 5, explOperands, ENC_FLAG_REXW, buf);
        }
        return write_modrm_r_imm8(0xc1, 5, explOperands, ENC_FLAG_REXW, buf);
    default:
        drob_assert_not_reached();
    }
}

DEF_ENCODE_FN(sub)
{
    switch (opcode) {
    case Opcode::SUB8mr:
        return write_modrm_m_reg(0x28, explOperands, ENC_FLAG_NONE, buf, addr);
    case Opcode::SUB16mr:
        return write_modrm_m_reg(0x29, explOperands, ENC_FLAG_66, buf, addr);
    case Opcode::SUB32mr:
        return write_modrm_m_reg(0x29, explOperands, ENC_FLAG_NONE, buf, addr);
    case Opcode::SUB64mr:
        return write_modrm_m_reg(0x29, explOperands, ENC_FLAG_REXW, buf, addr);

    case Opcode::SUB8rr:
        return write_modrm_r_reg(0x28, explOperands, ENC_FLAG_NONE, buf);
    case Opcode::SUB16rr:
        return write_modrm_r_reg(0x29, explOperands, ENC_FLAG_66, buf);
    case Opcode::SUB32rr:
        return write_modrm_r_reg(0x29, explOperands, ENC_FLAG_NONE, buf);
    case Opcode::SUB64rr:
        return write_modrm_r_reg(0x29, explOperands, ENC_FLAG_REXW, buf);

    case Opcode::SUB8rm:
        return write_modrm_reg_m(0x28, explOperands, ENC_FLAG_NONE, buf, addr);
    case Opcode::SUB16rm:
        return write_modrm_reg_m(0x29, explOperands, ENC_FLAG_66, buf, addr);
    case Opcode::SUB32rm:
        return write_modrm_reg_m(0x29, explOperands, ENC_FLAG_NONE, buf, addr);
    case Opcode::SUB64rm:
        return write_modrm_reg_m(0x29, explOperands, ENC_FLAG_REXW, buf, addr);

    case Opcode::SUB8mi:
        return write_modrm_m_imm8(0x80, 5, explOperands, ENC_FLAG_NONE, buf,
                                   addr);
    case Opcode::SUB16mi:
        if (is_simm8(explOperands.op[1].imm.val)) {
            return write_modrm_m_imm8(0x83, 5, explOperands, ENC_FLAG_66, buf,
                                      addr);
        }
        return write_modrm_m_imm16(0x81, 5, explOperands, ENC_FLAG_66, buf,
                                   addr);
    case Opcode::SUB32mi:
        if (is_simm8(explOperands.op[1].imm.val)) {
            return write_modrm_m_imm8(0x83, 5, explOperands, ENC_FLAG_NONE, buf,
                                      addr);
        }
        return write_modrm_m_imm32(0x81, 5, explOperands, ENC_FLAG_NONE, buf,
                                   addr);
    case Opcode::SUB64mi:
        if (is_simm8(explOperands.op[1].imm.val)) {
            return write_modrm_m_imm8(0x83, 5, explOperands, ENC_FLAG_REXW, buf,
                                      addr);
        }
        return write_modrm_m_imm32(0x81, 5, explOperands, ENC_FLAG_REXW, buf,
                                   addr);

    case Opcode::SUB8ri:
        if (explOperands.op[0].reg == Register::AL) {
            return write_imm8(0x2c, explOperands.op[1].imm.val, ENC_FLAG_NONE, buf);
        }
        return write_modrm_r_imm8(0x80, 5, explOperands, ENC_FLAG_NONE, buf);
    case Opcode::SUB16ri:
        if (is_simm8(explOperands.op[1].imm.val)) {
            return write_modrm_r_imm8(0x83, 5, explOperands, ENC_FLAG_66, buf);
        } else if (explOperands.op[0].reg == Register::AX) {
            return write_imm16(0x2d, explOperands.op[1].imm.val, ENC_FLAG_66, buf);
        }
        return write_modrm_r_imm16(0x81, 5, explOperands, ENC_FLAG_66, buf);
    case Opcode::SUB32ri:
        if (is_simm8(explOperands.op[1].imm.val)) {
            return write_modrm_r_imm8(0x83, 5, explOperands, ENC_FLAG_NONE, buf);
        } else if (explOperands.op[0].reg == Register::EAX) {
            return write_imm32(0x2d, explOperands.op[1].imm.val, ENC_FLAG_NONE, buf);
        }
        return write_modrm_r_imm32(0x81, 5, explOperands, ENC_FLAG_NONE, buf);
    case Opcode::SUB64ri:
        if (is_simm8(explOperands.op[1].imm.val)) {
            return write_modrm_r_imm8(0x83, 5, explOperands, ENC_FLAG_REXW, buf);
        } else if (explOperands.op[0].reg == Register::RAX) {
            return write_imm32(0x2d, explOperands.op[1].imm.val, ENC_FLAG_REXW, buf);
        }
        return write_modrm_r_imm32(0x81, 5, explOperands, ENC_FLAG_REXW, buf);
    default:
        drob_assert_not_reached();
    }
}

DEF_ENCODE_FN(ret)
{
    (void)opcode;
    (void)explOperands;
    (void)addr;
    *buf = 0xc3;
    return 1;
}

DEF_ENCODE_FN(test)
{
    switch (opcode) {
    case Opcode::TEST8mr:
        return write_modrm_m_reg(0x84, explOperands, ENC_FLAG_NONE, buf, addr);
    case Opcode::TEST8mi:
        return write_modrm_m_imm8(0xf6, 0, explOperands, ENC_FLAG_NONE, buf,
                                  addr);
    case Opcode::TEST8rr:
        return write_modrm_r_reg(0x84, explOperands, ENC_FLAG_66, buf);
    case Opcode::TEST8ri:
        if (explOperands.op[0].reg == Register::AL) {
            return write_imm8(0xa8, explOperands.op[1].imm.val, ENC_FLAG_66, buf);
        }
        return write_modrm_r_imm8(0xf6, 0, explOperands, ENC_FLAG_NONE, buf);
    case Opcode::TEST16mr:
        return write_modrm_m_reg(0x85, explOperands, ENC_FLAG_66, buf, addr);
    case Opcode::TEST16mi:
        return write_modrm_m_imm16(0xf7, 0, explOperands, ENC_FLAG_66, buf, addr);
    case Opcode::TEST16rr:
        return write_modrm_r_reg(0x85, explOperands, ENC_FLAG_66, buf);
    case Opcode::TEST16ri:
        if (explOperands.op[0].reg == Register::AX) {
            return write_imm16(0xa9, explOperands.op[1].imm.val, ENC_FLAG_66, buf);
        }
        return write_modrm_r_imm16(0xf7, 0, explOperands, ENC_FLAG_66, buf);
    case Opcode::TEST32mr:
        return write_modrm_m_reg(0x85, explOperands, ENC_FLAG_NONE, buf, addr);
    case Opcode::TEST32mi:
        return write_modrm_m_imm32(0xf7, 0, explOperands, ENC_FLAG_NONE, buf,
                                   addr);
    case Opcode::TEST32rr:
        return write_modrm_r_reg(0x85, explOperands, ENC_FLAG_NONE, buf);
    case Opcode::TEST32ri:
        if (explOperands.op[0].reg == Register::EAX) {
            return write_imm32(0xa9, explOperands.op[1].imm.val, ENC_FLAG_NONE, buf);
        }
        return write_modrm_r_imm32(0xf7, 0, explOperands, ENC_FLAG_NONE, buf);
    case Opcode::TEST64mr:
        return write_modrm_m_reg(0x85, explOperands, ENC_FLAG_REXW, buf, addr);
    case Opcode::TEST64mi:
        return write_modrm_m_imm32(0xf7, 0, explOperands, ENC_FLAG_REXW, buf,
                                   addr);
    case Opcode::TEST64rr:
        return write_modrm_r_reg(0x85, explOperands, ENC_FLAG_REXW, buf);
    case Opcode::TEST64ri:
        if (explOperands.op[0].reg == Register::RAX) {
            return write_imm32(0xa9, explOperands.op[1].imm.val, ENC_FLAG_REXW, buf);
        }
        return write_modrm_r_imm32(0xf7, 0, explOperands, ENC_FLAG_REXW, buf);
    default:
        drob_assert_not_reached();
    }
}

DEF_ENCODE_FN(xor)
{
    switch (opcode) {
    case Opcode::XOR64mr:
        return write_modrm_m_reg(0x31, explOperands, ENC_FLAG_REXW, buf, addr);
    case Opcode::XOR64rr:
        return write_modrm_r_reg(0x31, explOperands, ENC_FLAG_REXW, buf);
    case Opcode::XOR64rm:
        return write_modrm_reg_m(0x33, explOperands, ENC_FLAG_REXW, buf, addr);
    case Opcode::XOR64mi:
        if (is_simm8(explOperands.op[1].imm.val)) {
            return write_modrm_m_imm8(0x83, 6, explOperands, ENC_FLAG_REXW, buf,
                                      addr);
        }
        return write_modrm_m_imm32(0x81, 6, explOperands, ENC_FLAG_REXW, buf,
                                   addr);
    case Opcode::XOR64ri:
        if (is_simm8(explOperands.op[1].imm.val)) {
            return write_modrm_r_imm8(0x83, 6, explOperands, ENC_FLAG_REXW, buf);
        } else if (explOperands.op[0].reg == Register::RAX) {
            return write_imm32(0x35, explOperands.op[1].imm.val, ENC_FLAG_REXW, buf);
        }
        return write_modrm_r_imm32(0x81, 6, explOperands, ENC_FLAG_REXW, buf);
    case Opcode::XOR32mr:
        return write_modrm_m_reg(0x31, explOperands, ENC_FLAG_NONE, buf, addr);
    case Opcode::XOR32rr:
        return write_modrm_r_reg(0x31, explOperands, ENC_FLAG_NONE, buf);
    case Opcode::XOR32rm:
        return write_modrm_reg_m(0x33, explOperands, ENC_FLAG_NONE, buf, addr);
    case Opcode::XOR32mi:
        if (is_simm8(explOperands.op[1].imm.val)) {
            return write_modrm_m_imm8(0x83, 6, explOperands, ENC_FLAG_NONE, buf,
                                      addr);
        }
        return write_modrm_m_imm32(0x81, 6, explOperands, ENC_FLAG_REXW, buf,
                                   addr);
    case Opcode::XOR32ri:
        if (is_simm8(explOperands.op[1].imm.val)) {
            return write_modrm_r_imm8(0x83, 6, explOperands, ENC_FLAG_NONE, buf);
        } else if (explOperands.op[0].reg == Register::RAX) {
            return write_imm32(0x35, explOperands.op[1].imm.val, ENC_FLAG_NONE, buf);
        }
        return write_modrm_r_imm32(0x81, 6, explOperands, ENC_FLAG_NONE, buf);
    default:
        drob_assert_not_reached();
    }
}

/* recommended NOPs by Intel (Vol. 2B 4-163, Table 4-12) */
static const uint8_t nops[10][9] =
        {
                [0] = { },
                [1] = { 0x90 },
                [2] = { 0x66, 0x90 },
                [3] = { 0x0f, 0x1f, 0x00 },
                [4] = { 0x0f, 0x1f, 0x40, 0x00 },
                [5] = { 0x0f, 0x1f, 0x44, 0x00, 0x00 },
                [6] = { 0x66, 0x0f, 0x1f, 0x44, 0x00, 0x00 },
                [7] = { 0x0f, 0x1f, 0x80, 0x00, 0x00, 0x00, 0x00 },
                [8] = { 0x0f, 0x1f, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00 },
                [9] = { 0x66, 0x0f, 0x1f, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00 }, };

void arch_fill_with_nops(uint8_t *start, int size)
{
    while (size) {
        if (size > 9) {
            memcpy(start, nops[9], 9);
            size -= 9;
            start += 9;
            continue;
        }
        memcpy(start, nops[size], size);
        break;
    }
}

void arch_fixup_call(const CallLocation &call, const uint8_t *target,
                     bool write)
{
    int32_t disp = target - (call.itext + call.ilen);
    drob_assert(call.ilen == 5);

    if (!write) {
        /* there are no shorter jumps than 5 byte (rel32) */
        return;
    }

    call.itext[0] = 0xe8;
    *((int32_t *)&call.itext[1]) = disp;
}

static void fixupBranch(const BranchLocation &branch, const uint8_t *target,
                        bool write)
{
    int32_t disp;

    if (!write) {
        /* can we use a short instruction? remember it for next time */
        disp = target - (branch.itext + 2);
        if (is_rel8(disp)) {
            branch.instr->setUseShortBranch(true);
        }
        return;
    }

    disp = target - (branch.itext + branch.ilen);
    if (branch.instr->getUseShortBranch()) {
        drob_assert(branch.ilen == 2);
        drob_assert(is_rel8(disp));
        branch.itext[0] = 0xeb;
        branch.itext[1] = (int8_t)disp;
    } else {
        drob_assert(branch.ilen == 5);
        branch.itext[0] = 0xe9;
        *((int32_t *)&branch.itext[1]) = disp;
    }
}

static void fixupSpecialCondBranch(const BranchLocation &branch,
                                   const uint8_t *target, bool write)
{
    int short_ilen = 2, long_ilen = 9, pos = 0;
    bool addr_prefix = false;
    uint8_t opcode;
    int32_t disp;

    switch (branch.instr->getOpcode()) {
    case Opcode::JCXZ64a:
        addr_prefix = true;
        /* FALL THROUGH */
    case Opcode::JCXZ32a:
        opcode = 0xe3;
        break;
    default:
        drob_assert_not_reached();
    }
    if (addr_prefix) {
        short_ilen++;
        long_ilen++;
    }

    if (!write) {
        /* can we use a short instruction? remember it for next time */
        disp = target - (branch.itext + short_ilen);
        if (is_rel8(disp)) {
            branch.instr->setUseShortBranch(true);
        }
        return;
    }

    if (branch.instr->getUseShortBranch()) {
        drob_assert(short_ilen == branch.ilen);
        disp = target - (branch.itext + short_ilen);
        drob_assert(is_rel8(disp));

        if (addr_prefix) {
            branch.itext[pos] = 0x67;
        }
        branch.itext[pos++] = opcode;
        branch.itext[pos++] = (int8_t)disp;
    } else {
        drob_assert(long_ilen == branch.ilen);
        /* short jump to the near branch instruction conditionally */
        if (addr_prefix) {
            branch.itext[pos] = 0x67;
        }
        branch.itext[pos++] = opcode;
        branch.itext[pos++] = (int8_t)2;
        /* short jump over the near branch instruction */
        branch.itext[pos++] = 0xeb;
        branch.itext[pos++] = (int8_t)5;
        /* near jump to the actual target */
        branch.itext[pos++] = 0xe9;
        disp = target - (&branch.itext[pos] + 5);
        *((int32_t *)&branch.itext[pos]) = disp;
    }
}

void arch_fixup_branch(const BranchLocation &branch, const uint8_t *target,
                       bool write)
{
    uint8_t opcode;
    int32_t disp;

    switch (branch.instr->getOpcode()) {
    case Opcode::JOa:
        opcode = 0x80;
        break;
    case Opcode::JNOa:
        opcode = 0x81;
        break;
    case Opcode::JBa:
        opcode = 0x82;
        break;
    case Opcode::JNBa:
        opcode = 0x83;
        break;
    case Opcode::JZa:
        opcode = 0x84;
        break;
    case Opcode::JNZa:
        opcode = 0x85;
        break;
    case Opcode::JBEa:
        opcode = 0x86;
        break;
    case Opcode::JNBEa:
        opcode = 0x87;
        break;
    case Opcode::JSa:
        opcode = 0x88;
        break;
    case Opcode::JNSa:
        opcode = 0x89;
        break;
    case Opcode::JPa:
        opcode = 0x8a;
        break;
    case Opcode::JNPa:
        opcode = 0x8b;
        break;
    case Opcode::JLa:
        opcode = 0x8c;
        break;
    case Opcode::JNLa:
        opcode = 0x8d;
        break;
    case Opcode::JLEa:
        opcode = 0x8e;
        break;
    case Opcode::JNLEa:
        opcode = 0x8f;
        break;
    case Opcode::JMPa:
    case Opcode::JMPr:
    case Opcode::JMPm:
        return fixupBranch(branch, target, write);
    case Opcode::JCXZ32a:
    case Opcode::JCXZ64a:
        return fixupSpecialCondBranch(branch, target, write);
    default:
        drob_assert_not_reached();
    }

    /* handling for simple conditional branches */
    if (!write) {
        /* can we use a short instruction? remember it for next time */
        disp = target - (branch.itext + 2);
        if (is_rel8(disp)) {
            branch.instr->setUseShortBranch(true);
        }
        return;
    }

    disp = target - (branch.itext + branch.ilen);
    if (branch.instr->getUseShortBranch()) {
        drob_assert(branch.ilen == 2);
        drob_assert(is_rel8(disp));
        branch.itext[0] = opcode - 0x10;
        branch.itext[1] = (int8_t)disp;
    } else {
        drob_assert(branch.ilen == 6);
        branch.itext[0] = 0x0f;
        branch.itext[1] = opcode;
        *((int32_t *)&branch.itext[2]) = disp;
    }
}

static BranchLocation prepareCondBranch(Instruction &instr,
                                        BinaryPool &binaryPool)
{
    /* 6 bytes are enough for 32 bit displacement */
    BranchLocation branch = { .itext = nullptr, .ilen = 6, .instr = &instr, };

    /* 2 bytes are enough for 8 bit displacement */
    if (instr.getUseShortBranch()) {
        branch.ilen = 2;
    }
    branch.itext = binaryPool.allocCode(branch.ilen);

    return branch;
}

static BranchLocation prepareSpecialCondBranch(Instruction &instr,
                                               BinaryPool &binaryPool)
{
    BranchLocation branch = { .itext = nullptr, .ilen = 0, .instr = &instr, };

    switch (instr.getOpcode()) {
    case Opcode::JCXZ32a:
        /* rel8 + JMP rel8 + JMP rel32 */
        branch.ilen = 9;
        /* rel8 only */
        if (instr.getUseShortBranch()) {
            branch.ilen = 2;
        }
        break;
    case Opcode::JCXZ64a:
        /* rel8 + JMP rel8 + JMP rel32 */
        branch.ilen = 10;
        /* rel8 only */
        if (instr.getUseShortBranch()) {
            branch.ilen = 3;
        }
        break;
    default:
        drob_assert_not_reached();
    }

    branch.itext = binaryPool.allocCode(branch.ilen);
    return branch;
}

BranchLocation arch_prepare_branch(Instruction &instr, BinaryPool &binaryPool)
{
    switch (instr.getOpcode()) {
    case Opcode::JNBEa:
    case Opcode::JNBa:
    case Opcode::JBa:
    case Opcode::JBEa:
    case Opcode::JZa:
    case Opcode::JNLEa:
    case Opcode::JNLa:
    case Opcode::JLa:
    case Opcode::JLEa:
    case Opcode::JNZa:
    case Opcode::JNOa:
    case Opcode::JNPa:
    case Opcode::JNSa:
    case Opcode::JOa:
    case Opcode::JPa:
    case Opcode::JSa:
        return prepareCondBranch(instr, binaryPool);
    case Opcode::JCXZ32a:
    case Opcode::JCXZ64a:
        return prepareSpecialCondBranch(instr, binaryPool);
    case Opcode::JMPa:
    case Opcode::JMPr:
    case Opcode::JMPm: {
        /* 5 bytes are enough for 32 bit displacement */
        BranchLocation branch =
                { .itext = nullptr, .ilen = 5, .instr = &instr, };

        /* 2 bytes are enough for 8 bit displacement */
        if (instr.getUseShortBranch()) {
            branch.ilen = 2;
        }
        branch.itext = binaryPool.allocCode(branch.ilen);

        return branch;
    }
    default:
        drob_assert_not_reached();
    };
}

CallLocation arch_prepare_call(Instruction &instr, BinaryPool &binaryPool)
{
    /* 5 bytes are enough for 32 bit displacement */
    CallLocation call = { .itext = binaryPool.allocCode(5), .ilen = 5, .instr =
            &instr, };

    return call;
}

} /* namespace drob */
