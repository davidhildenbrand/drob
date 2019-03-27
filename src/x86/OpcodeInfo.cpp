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
#include "../Instruction.hpp"
#include "OpcodeInfo.hpp"
#include "Converter.hpp"
#include "Predicate.hpp"
#include "Encoder.hpp"
#include "Emulator.hpp"
#include "Specialize.hpp"

namespace drob {

#define refine_nullptr nullptr
#define encode_nullptr nullptr
#define emulate_nullptr nullptr
#define specialize_nullptr nullptr

/*
 * Placeholders for empty definitions to calculate the operand count.
 */
static const ExplicitStaticOperandInfo eoi_none[0] = {
};
static const StaticOperandInfo ioi_none[0] = {
};

/*
 * Placeholder to get the address of the predicate.
 */
static const Predicate pred_none = {
};

#define DEF_REG_READ(_type, _access) \
    { \
        .type = OperandType::Register, \
        .r = { \
            .type = RegisterType::_type, \
            .mode = AccessMode::Read, \
            .r = RegisterAccessType::_access, \
            .w = RegisterAccessType::None, \
        }, \
    }

#define DEF_REG_MAYREAD(_type, _access) \
    { \
        .type = OperandType::Register, \
        .r = { \
            .type = RegisterType::_type, \
            .mode = AccessMode::MayRead, \
            .r = RegisterAccessType::_access, \
            .w = RegisterAccessType::None, \
        }, \
    }

#define DEF_REG_WRITE(_type, _access) \
    { \
        .type = OperandType::Register, \
        .r = { \
            .type = RegisterType::_type, \
            .mode = AccessMode::Write, \
            .r = RegisterAccessType::None, \
            .w = RegisterAccessType::_access, \
        }, \
    }

#define DEF_REG_READWRITE_DIFF(_type, _raccess, _waccess) \
    { \
        .type = OperandType::Register, \
        .r = { \
            .type = RegisterType::_type, \
            .mode = AccessMode::ReadWrite, \
            .r = RegisterAccessType::_raccess, \
            .w = RegisterAccessType::_waccess, \
        }, \
    }

#define DEF_REG_READWRITE(_type, _access) \
    DEF_REG_READWRITE_DIFF(_type, _access, _access)

#define DEF_REG_MAYREADWRITE_DIFF(_type, _raccess, _waccess) \
    { \
        .type = OperandType::Register, \
        .r = { \
            .type = RegisterType::_type, \
            .mode = AccessMode::MayReadWrite, \
            .r = RegisterAccessType::_raccess, \
            .w = RegisterAccessType::_waccess, \
        }, \
    }

#define DEF_REG_MAYREADWRITE(_type, _access) \
    DEF_REG_MAYREADWRITE_DIFF(_type, _access, _access) \

#define DEF_MEM(_bytes, _access) \
    { \
        .type = OperandType::MemPtr, \
        .m = { .mode = AccessMode::_access, \
               .size = MemAccessSize::B##_bytes }, \
    }

#define DEF_MEM_READ(_bytes) \
    DEF_MEM(_bytes, Read)

#define DEF_MEM_MAYREAD(_bytes) \
    DEF_MEM(_bytes, MayRead)

#define DEF_MEM_WRITE(_bytes) \
    DEF_MEM(_bytes, Write)

#define DEF_MEM_READWRITE(_bytes) \
    DEF_MEM(_bytes, ReadWrite)

#define DEF_MEM_MAYREADWRITE(_bytes) \
    DEF_MEM(_bytes, MayReadWrite)

#define DEF_MEM_ADDRESS() \
    { \
        .type = OperandType::MemPtr, \
        .m = { .mode = AccessMode::Address, \
               .size = MemAccessSize::Unknown }, \
    }

#define DEF_SIMM(_bits) \
    { \
        .type = OperandType::SignedImmediate##_bits, \
        .r = {}, \
    }

#define DEF_IMM(_bits) \
    { \
        .type = OperandType::Immediate##_bits, \
        .r = {}, \
    }

/* memory address operand definitions */
#define mA          DEF_MEM_ADDRESS()

/* 8-bit memory operand definitions */
#define m8R         DEF_MEM_READ(1)
#define m8W         DEF_MEM_WRITE(1)
#define m8RW        DEF_MEM_READWRITE(1)

/* 16-bit memory operand definitions */
#define m16R        DEF_MEM_READ(2)
#define m16W        DEF_MEM_WRITE(2)
#define m16RW       DEF_MEM_READWRITE(2)

/* 32-bit memory operand definitions */
#define m32R        DEF_MEM_READ(4)
#define m32W        DEF_MEM_WRITE(4)
#define m32RW       DEF_MEM_READWRITE(4)

/* 64-bit memory operand definitions */
#define m64R        DEF_MEM_READ(8)
#define m64W        DEF_MEM_WRITE(8)
#define m64RW       DEF_MEM_READWRITE(8)

/* 128-bit memory operand definitions */
#define m128R       DEF_MEM_READ(16)
#define m128W       DEF_MEM_WRITE(16)

/* 8-bit gprs register operand definitions */
#define r8R         DEF_REG_READ(Gprs8, Full)
#define r8W         DEF_REG_WRITE(Gprs8, Full)
#define r8RW        DEF_REG_READWRITE(Gprs8, Full)

/* 16-bit gprs register operand definitions */
#define r16R        DEF_REG_READ(Gprs16, Full)
#define r16W        DEF_REG_WRITE(Gprs16, Full)
#define r16RW       DEF_REG_READWRITE(Gprs16, Full)

/* 32-bit gprs register operand definitions */
#define r32R        DEF_REG_READ(Gprs32, Full)
#define r32W        DEF_REG_WRITE(Gprs32, FullZeroParent)
#define r32RW       DEF_REG_READWRITE_DIFF(Gprs32, Full, FullZeroParent)
#define r32MR       DEF_REG_MAYREAD(Gprs32, Full)
#define r32MRW      DEF_REG_MAYREADWRITE_DIFF(Gprs32, Full, FullZeroParent)

/* 64-bit gprs register operand definitions */
#define r64R        DEF_REG_READ(Gprs64, Full)
#define r64W        DEF_REG_WRITE(Gprs64, Full)
#define r64RW       DEF_REG_READWRITE(Gprs64, Full)
#define r64MR       DEF_REG_MAYREAD(Gprs64, Full)
#define r64MRW      DEF_REG_MAYREADWRITE(Gprs64, Full)

/* 64-bit sse register operand definitions */
#define x64W        DEF_REG_WRITE(Sse128, H0)
#define x64R        DEF_REG_READ(Sse128, H0)
#define x64RW       DEF_REG_READWRITE(Sse128, H0)
#define x64MR       DEF_REG_MAYREAD(Sse128, H0)
#define x64MRW      1DEF_REG_MAYREADWRITE(Sse128, H0)

/* 128-bit sse register operand definitions */
#define x128W       DEF_REG_WRITE(Sse128, Full)
#define x128R       DEF_REG_READ(Sse128, Full)
#define x128RW      DEF_REG_READWRITE(Sse128, Full)
#define x128MR      DEF_REG_MAYREAD(Sse128, Full)
#define x128MRW     DEF_REG_MAYREADWRITE(Sse128, Full)

/* Immediate operand definitions */
#define i8          DEF_IMM(8)
#define i16         DEF_IMM(16)
#define i32         DEF_IMM(32)
#define i64         DEF_IMM(64)
#define s32         DEF_SIMM(32)

/* Helpers to define explicit memory operands */
#define DEF_EOI_1(_OP0) \
    static const ExplicitStaticOperandInfo eoi_##_OP0[1] = { \
        _OP0, \
    }
#define DEF_EOI_2(_OP0, _OP1) \
    static const ExplicitStaticOperandInfo eoi_##_OP0##_##_OP1[2] = { \
        _OP0, \
        _OP1, \
    }

/* Definitions for instructions with 1 explicit memory operand */
DEF_EOI_1(r16R);
DEF_EOI_1(r16W);

DEF_EOI_1(m16R);
DEF_EOI_1(m16W);

DEF_EOI_1(r64R);
DEF_EOI_1(r64W);
DEF_EOI_1(r64RW);

DEF_EOI_1(m64R);
DEF_EOI_1(m64W);
DEF_EOI_1(m64RW);
DEF_EOI_1(mA);

DEF_EOI_1(i16);
DEF_EOI_1(s32);

/* Definitions for instructions with 2 explicit memory operands */
DEF_EOI_2(r8R, r8R);
DEF_EOI_2(r8R, m8R);
DEF_EOI_2(r8R, i8);
DEF_EOI_2(r8RW, r8R);
DEF_EOI_2(r8RW, m8R);
DEF_EOI_2(r8RW, i8);
DEF_EOI_2(r8W, mA);

DEF_EOI_2(m8R, r8R);
DEF_EOI_2(m8R, i8);
DEF_EOI_2(m8RW, r8R);
DEF_EOI_2(m8RW, i8);

DEF_EOI_2(r16R, r16R);
DEF_EOI_2(r16R, m16R);
DEF_EOI_2(r16R, i16);
DEF_EOI_2(r16RW, r16R);
DEF_EOI_2(r16RW, m16R);
DEF_EOI_2(r16RW, i16);
DEF_EOI_2(r16W, mA);

DEF_EOI_2(m16R, r16R);
DEF_EOI_2(m16R, i16);
DEF_EOI_2(m16RW, r16R);
DEF_EOI_2(m16RW, i16);

DEF_EOI_2(r32R, r32R);
DEF_EOI_2(r32R, m32R);
DEF_EOI_2(r32R, i32);
DEF_EOI_2(r32W, r32R);
DEF_EOI_2(r32W, m32R);
DEF_EOI_2(r32W, i32);
DEF_EOI_2(r32W, mA);
DEF_EOI_2(r32RW, r32R);
DEF_EOI_2(r32RW, m32R);
DEF_EOI_2(r32RW, i32);
DEF_EOI_2(r32MRW, r32MR);

DEF_EOI_2(m32R, r32R);
DEF_EOI_2(m32R, i32);
DEF_EOI_2(m32W, i32);
DEF_EOI_2(m32W, r32R);
DEF_EOI_2(m32RW, i32);
DEF_EOI_2(m32RW, r32R);

DEF_EOI_2(r64R, r64R);
DEF_EOI_2(r64R, m64R);
DEF_EOI_2(r64R, s32);
DEF_EOI_2(r64W, r64R);
DEF_EOI_2(r64W, m64R);
DEF_EOI_2(r64W, i64);
DEF_EOI_2(r64W, mA);
DEF_EOI_2(r64RW, r64R);
DEF_EOI_2(r64RW, m64R);
DEF_EOI_2(r64RW, s32);
DEF_EOI_2(r64RW, i8);
DEF_EOI_2(r64MRW, r64MR);

DEF_EOI_2(m64R, r64R);
DEF_EOI_2(m64R, s32);
DEF_EOI_2(m64W, r64R);
DEF_EOI_2(m64W, s32);
DEF_EOI_2(m64W, i64);
DEF_EOI_2(m64W, x64R);
DEF_EOI_2(m64RW, r64R);
DEF_EOI_2(m64RW, i8);
DEF_EOI_2(m64RW, s32);

DEF_EOI_2(x64W, m64R);
DEF_EOI_2(x64W, x64R);
DEF_EOI_2(x64RW, m64R);
DEF_EOI_2(x64RW, x64R);

DEF_EOI_2(x128W, m128R);
DEF_EOI_2(x128W, x128R);
DEF_EOI_2(x128RW, m128R);
DEF_EOI_2(x128RW, x128R);
DEF_EOI_2(x128MRW, x128MR);

DEF_EOI_2(m128W, x128R);

/*
 * Write all eflags.
 */
static const StaticOperandInfo ioi_eflagsW[6] = {
    {
        .type = OperandType::Register,
        .r = { .reg = Register::CF,
               .mode = AccessMode::Write,
               .r = RegisterAccessType::None,
               .w = RegisterAccessType::Full },
    },
    {
        .type = OperandType::Register,
        .r = { .reg = Register::PF,
               .mode = AccessMode::Write,
               .r = RegisterAccessType::None,
               .w = RegisterAccessType::Full },
    },
    {
        .type = OperandType::Register,
        .r = { .reg = Register::AF,
               .mode = AccessMode::Write,
               .r = RegisterAccessType::None,
               .w = RegisterAccessType::Full },
    },
    {
        .type = OperandType::Register,
        .r = { .reg = Register::ZF,
               .mode = AccessMode::Write,
               .r = RegisterAccessType::None,
               .w = RegisterAccessType::Full },
    },
    {
        .type = OperandType::Register,
        .r = { .reg = Register::SF,
               .mode = AccessMode::Write,
               .r = RegisterAccessType::None,
               .w = RegisterAccessType::Full },
     },    {
        .type = OperandType::Register,
        .r = { .reg = Register::OF,
               .mode = AccessMode::Write,
               .r = RegisterAccessType::None,
               .w = RegisterAccessType::Full },
     },
};

/*
 * May write all eflags.
 */
static const StaticOperandInfo ioi_eflagsMW[6] = {
        {
            .type = OperandType::Register,
            .r = { .reg = Register::CF,
                   .mode = AccessMode::MayWrite,
                   .r = RegisterAccessType::None,
                   .w = RegisterAccessType::Full },
        },
        {
            .type = OperandType::Register,
            .r = { .reg = Register::PF,
                   .mode = AccessMode::MayWrite,
                   .r = RegisterAccessType::None,
                   .w = RegisterAccessType::Full },
        },
        {
            .type = OperandType::Register,
            .r = { .reg = Register::AF,
                   .mode = AccessMode::MayWrite,
                   .r = RegisterAccessType::None,
                   .w = RegisterAccessType::Full },
        },
        {
            .type = OperandType::Register,
            .r = { .reg = Register::ZF,
                   .mode = AccessMode::MayWrite,
                   .r = RegisterAccessType::None,
                   .w = RegisterAccessType::Full },
        },
        {
            .type = OperandType::Register,
            .r = { .reg = Register::SF,
                   .mode = AccessMode::MayWrite,
                   .r = RegisterAccessType::None,
                   .w = RegisterAccessType::Full },
         },    {
            .type = OperandType::Register,
            .r = { .reg = Register::OF,
                   .mode = AccessMode::MayWrite,
                   .r = RegisterAccessType::None,
                   .w = RegisterAccessType::Full },
         },
};

static const StaticOperandInfo ioi_pop64[2] = {
    {
        .type = OperandType::Register,
        .r = { .reg = Register::RSP,
               .mode = AccessMode::ReadWrite,
               .r = RegisterAccessType::Full,
               .w = RegisterAccessType::Full },
    },
    {
        .type = OperandType::MemPtr,
        .m = {
            .ptr = {
                .type = MemPtrType::SIB,
                .sib = {
                    .base = Register::RSP,
                    .index = Register::None,
                    .disp = {
                        .val = 0,
                        .usrPtrNr = -1,
                        .usrPtrOffset = 0,
                    },
                    .scale = 0,
                },
            },
            .mode = AccessMode::Read,
            .size = MemAccessSize::B8,
        },
    },
};

static const StaticOperandInfo ioi_pop16[2] = {
    {
        .type = OperandType::Register,
        .r = { .reg = Register::RSP,
               .mode = AccessMode::ReadWrite,
               .r = RegisterAccessType::Full,
               .w = RegisterAccessType::Full },
    },
    {
        .type = OperandType::MemPtr,
        .m = {
            .ptr = {
                .type = MemPtrType::SIB,
                .sib = {
                    .base = Register::RSP,
                    .index = Register::None,
                    .disp = {
                        .val = 0,
                        .usrPtrNr = -1,
                        .usrPtrOffset = 0,
                    },
                    .scale = 0,
                },
            },
            .mode = AccessMode::Read,
            .size = MemAccessSize::B2,
        },
    },
};


static const StaticOperandInfo ioi_ret[2] = {
    {
        .type = OperandType::Register,
        .r = { .reg = Register::RSP,
               .mode = AccessMode::ReadWrite,
               .r = RegisterAccessType::Full,
               .w = RegisterAccessType::Full },
    },
    {
        .type = OperandType::MemPtr,
        .m = {
            .ptr = {
                .type = MemPtrType::SIB,
                .sib = {
                    .base = Register::RSP,
                    .index = Register::None,
                    .disp = {
                        .val = 0,
                        .usrPtrNr = -1,
                        .usrPtrOffset = 0,
                    },
                    .scale = 0,
                },
            },
            .mode = AccessMode::Read,
            .size = MemAccessSize::B8,
        },
    },
};

static const StaticOperandInfo ioi_push64[2] = {
    {
        .type = OperandType::Register,
        .r = { .reg = Register::RSP,
               .mode = AccessMode::ReadWrite,
               .r = RegisterAccessType::Full,
               .w = RegisterAccessType::Full },
    },
    {
        .type = OperandType::MemPtr,
        .m = {
            .ptr = {
                .type = MemPtrType::SIB,
                .sib = {
                    .base = Register::RSP,
                    .index = Register::None,
                    .disp = {
                        .val = -8,
                        .usrPtrNr = -1,
                        .usrPtrOffset = 0,
                    },
                    .scale = 0,
                },
            },
            .mode = AccessMode::Write,
            .size = MemAccessSize::B8,
        },
    },
};

static const StaticOperandInfo ioi_push16[2] = {
    {
        .type = OperandType::Register,
        .r = { .reg = Register::RSP,
               .mode = AccessMode::ReadWrite,
               .r = RegisterAccessType::Full,
               .w = RegisterAccessType::Full },
    },
    {
        .type = OperandType::MemPtr,
        .m = {
            .ptr = {
                .type = MemPtrType::SIB,
                .sib = {
                    .base = Register::RSP,
                    .index = Register::None,
                    .disp = {
                        .val = -2,
                        .usrPtrNr = -1,
                        .usrPtrOffset = 0,
                    },
                    .scale = 0,
                },
            },
            .mode = AccessMode::Write,
            .size = MemAccessSize::B2,
        },
    },
};

static const StaticOperandInfo ioi_sh[7] = {
    {
        .type = OperandType::Register,
        .r = { .reg = Register::CL,
               .mode = AccessMode::Read,
               .r = RegisterAccessType::None,
               .w = RegisterAccessType::Full },
    },
    /* "if the count is 0, the flags are not affacted" - MayWrite */
    {
        .type = OperandType::Register,
        .r = { .reg = Register::CF,
               .mode = AccessMode::MayWrite,
               .r = RegisterAccessType::None,
               .w = RegisterAccessType::Full },
    },
    {
        .type = OperandType::Register,
        .r = { .reg = Register::PF,
               .mode = AccessMode::MayWrite,
               .r = RegisterAccessType::None,
               .w = RegisterAccessType::Full },
    },
    {
        .type = OperandType::Register,
        .r = { .reg = Register::AF,
               .mode = AccessMode::MayWrite,
               .r = RegisterAccessType::None,
               .w = RegisterAccessType::Full },
    },
    {
        .type = OperandType::Register,
        .r = { .reg = Register::ZF,
               .mode = AccessMode::MayWrite,
               .r = RegisterAccessType::None,
               .w = RegisterAccessType::Full },
    },
    {
        .type = OperandType::Register,
        .r = { .reg = Register::SF,
               .mode = AccessMode::MayWrite,
               .r = RegisterAccessType::None,
               .w = RegisterAccessType::Full },
     },    {
        .type = OperandType::Register,
        .r = { .reg = Register::OF,
               .mode = AccessMode::MayWrite,
               .r = RegisterAccessType::None,
               .w = RegisterAccessType::Full },
     },
};


#define DEF_OPC(_NAME, _EOI, _IOI, _PRED, _TYPE, _REF, _ENC, _EMU, _SPEC, _FLAGS) \
const OpcodeInfo opcode_##_NAME { \
    .numOperands = ARRAY_SIZE(eoi_##_EOI), \
    .numImplOperands = ARRAY_SIZE(ioi_##_IOI), \
    .type = OpcodeType::_TYPE, \
    .unused = 0, \
    .opInfo = eoi_##_EOI, \
    .iOpInfo = ioi_##_IOI, \
    .predicate = &pred_##_PRED == &pred_none ? nullptr : &pred_##_PRED, \
    .refine = refine_##_REF, \
    .encode = encode_##_ENC, \
    .emulate = emulate_##_EMU, \
    .specialize = specialize_##_SPEC, \
    .flags = _FLAGS, \
};

#include "Opcodes.inc.h"
#undef DEF_OPC

#define DEF_OPC(_OPC, ...) \
    [static_cast<uint16_t>(Opcode::_OPC)] = &opcode_##_OPC,
#define DEF_OPC_NULL(_OPC) \
    [static_cast<uint16_t>(Opcode::_OPC)] = nullptr,

const OpcodeInfo *const oci[static_cast<uint16_t>(Opcode::MAX)] = {
    DEF_OPC_NULL(NONE)
#include "Opcodes.inc.h"
};
#undef DEF_OPC

/*
 * Lookup the opcode information for an instruction.
 * (Opcode order, so our compiler properly optimizes this).
 */
const OpcodeInfo *arch_get_opcode_info(Opcode opc)
{
    return oci[static_cast<uint16_t>(opc)];
}

DEF_REFINE_FN(sh)
{
    switch (opcode) {
    case Opcode::SHL64mi:
    case Opcode::SHL64ri:
    case Opcode::SHR64mi:
    case Opcode::SHR64ri:
        if (operandInfo.isImpl) {
            drob_assert(operandInfo.type == OperandType::Register);
            if (explOperands.op[1].imm.val == 0) {
                /* if the shift is 0, eflags are not touched */
                operandInfo.r.mode = AccessMode::None;
            } else {
                /* all eflags are written but might be unpredictable (handled by emulator) */
                operandInfo.r.mode = AccessMode::Write;
            }
        }
        break;
    case Opcode::SHL64r:
    case Opcode::SHL64m:
    case Opcode::SHR64r:
    case Opcode::SHR64m:
        /* CL is at idx 0 */
        if (operandInfo.nr > 1 && ps) {
            drob_assert(operandInfo.type == OperandType::Register);

            Data data = ps->getRegister(Register::CL);
            if (data.isImm()) {
                if (data.getImm64() == 0) {
                    /* if the shift is 0, eflags are not touched */
                    operandInfo.r.mode = AccessMode::None;
                } else {
                    /* all eflags are written but might be unpredictable */
                    operandInfo.r.mode = AccessMode::Write;
                }
            }
        }
        break;
    default:
        drob_assert_not_reached();
    }
}

DEF_REFINE_FN(xor_rr)
{
    drob_assert(opcode == Opcode::PXOR128rr || opcode == Opcode::XOR64rr ||
                opcode == Opcode::XOR32rr);
    if (operandInfo.nr == 0) {
        if (explOperands.op[0].reg == explOperands.op[1].reg) {
            operandInfo.r.mode = AccessMode::Write;
        } else {
            operandInfo.r.mode = AccessMode::ReadWrite;
        }
    } else if (operandInfo.nr == 1) {
        if (explOperands.op[0].reg == explOperands.op[1].reg) {
            operandInfo.r.mode = AccessMode::None;
        } else {
            operandInfo.r.mode = AccessMode::Read;
        }
    }
}

Opcode arch_invert_branch(Opcode opcode)
{
    switch(opcode) {
    case Opcode::JNBEa:
        return Opcode::JBEa;
    case Opcode::JNBa:
        return Opcode::JBa;
    case Opcode::JBa:
        return Opcode::JNBa;
    case Opcode::JBEa:
        return Opcode::JNBEa;
    case Opcode::JZa:
        return Opcode::JNZa;
    case Opcode::JNLEa:
        return Opcode::JLEa;
    case Opcode::JNLa:
        return Opcode::JLa;
    case Opcode::JLa:
        return Opcode::JNLa;
    case Opcode::JLEa:
        return Opcode::JNLEa;
    case Opcode::JNZa:
        return Opcode::JZa;
    case Opcode::JNOa:
        return Opcode::JOa;
    case Opcode::JNPa:
        return Opcode::JPa;
    case Opcode::JNSa:
        return Opcode::JSa;
    case Opcode::JOa:
        return Opcode::JNOa;
    case Opcode::JPa:
        return Opcode::JNPa;
    case Opcode::JSa:
        return Opcode::JNSa;
    default:
        return Opcode::NONE;
    }
}

} /* namespace drob */
