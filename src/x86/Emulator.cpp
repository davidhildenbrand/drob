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
#include "../InstructionInfo.hpp"
#include "Emulator.hpp"
#include "x86.hpp"

namespace drob {

/*
 * We are using the default AT&T syntax here, so in contrast to out opcode
 * definitions, the operands are in reverse order.
 */

typedef struct Eflags {
    uint8_t cf : 1;
    uint8_t pf : 1;
    uint8_t af : 1;
    uint8_t zf : 1;
    uint8_t sf : 1;
    uint8_t of : 1;
} Eflags;

static void setEflags(DynamicInstructionInfo& dynInfo, int startIdx,
                      Eflags &eflags)
{
    drob_assert(dynInfo.operands[startIdx].type == OperandType::Register);
    drob_assert(dynInfo.operands[startIdx].regAcc.reg == Register::CF);
    dynInfo.operands[startIdx++].output = DynamicValue(eflags.cf);
    dynInfo.operands[startIdx++].output = DynamicValue(eflags.pf);
    dynInfo.operands[startIdx++].output = DynamicValue(eflags.af);
    dynInfo.operands[startIdx++].output = DynamicValue(eflags.zf);
    dynInfo.operands[startIdx++].output = DynamicValue(eflags.sf);
    dynInfo.operands[startIdx++].output = DynamicValue(eflags.of);
}

static void setAF(DynamicInstructionInfo& dynInfo, int idx, const DynamicValue &data)
{
    drob_assert(dynInfo.operands[idx].type == OperandType::Register);
    drob_assert(dynInfo.operands[idx].regAcc.reg == Register::AF);
    dynInfo.operands[idx].output = data;
}

static void setCF(DynamicInstructionInfo& dynInfo, int idx, const DynamicValue &data)
{
    drob_assert(dynInfo.operands[idx].type == OperandType::Register);
    drob_assert(dynInfo.operands[idx].regAcc.reg == Register::CF);
    dynInfo.operands[idx].output = data;
}

static void setOF(DynamicInstructionInfo& dynInfo, int idx, const DynamicValue &data)
{
    drob_assert(dynInfo.operands[idx].type == OperandType::Register);
    drob_assert(dynInfo.operands[idx].regAcc.reg == Register::OF);
    dynInfo.operands[idx].output = data;
}

static inline void readEflags(Eflags &eflags)
{
    uint16_t flags;

    asm volatile ("     lahf\n"
                  "     seto %%al\n"
                  : "=a"(flags)
                  :: "memory" );
    eflags.cf = (flags >> 8) & 1;
    eflags.pf = (flags >> 10) & 1;
    eflags.af = (flags >> 12) & 1;
    eflags.zf = (flags >> 14) & 1;
    eflags.sf = (flags >> 15) & 1;
    eflags.of = flags & 1;
    drob_assert(flags & 0x200);
}

#define GEN_ADD(BITS, C) \
static inline uint##BITS##_t add##BITS(uint##BITS##_t a, uint##BITS##_t b, \
                                       Eflags &eflags) \
{ \
    asm volatile ("     add" #C " %[in], %[inout]\n" \
                  : [inout] "+g" (a) \
                  : [in] "r" (b) \
                  : "memory" ); \
    readEflags(eflags); \
    return a; \
}
GEN_ADD(8, b)
GEN_ADD(16, w)
GEN_ADD(32, l)
GEN_ADD(64, q)

#define GEN_EMULATE_ADD(BITS) \
DEF_EMULATE_FN(add##BITS) \
{ \
    DynamicOperandInfo &inout = dynInfo.operands[0]; \
    DynamicOperandInfo &in = dynInfo.operands[1]; \
    Eflags eflags; \
\
    inout.output = add##BITS(inout.input.getImm64(), in.input.getImm64(), eflags);\
    setEflags(dynInfo, 2, eflags); \
    return EmuRet::Ok;\
}
GEN_EMULATE_ADD(8)
GEN_EMULATE_ADD(16)
GEN_EMULATE_ADD(32)

DEF_EMULATE_FN(add64)
{
    DynamicOperandInfo &inout = dynInfo.operands[0];
    DynamicOperandInfo &in = dynInfo.operands[1];
    Eflags eflags;

    /* Outputs (and therefore flags) are marked as unknown as default */
    if (inout.input.isImm() && in.input.isImm()) {
        inout.output = add64(inout.input.getImm64(), in.input.getImm64(), eflags);
        setEflags(dynInfo, 2, eflags);
    } else if (inout.input.isPtr() && in.input.isImm()) {
        int64_t offs = add64(inout.input.getPtrOffset(), in.input.getImm64(), eflags);

        inout.output = DynamicValue(inout.input.getType(), inout.input.getNr(), offs);
    } else if (in.input.isPtr() && inout.input.isImm()) {
        int64_t offs = add64(in.input.getPtrOffset(), inout.input.getImm64(), eflags);

        inout.output = DynamicValue(in.input.getType(), in.input.getNr(), offs);
    } else if (inout.input.isStackPtr() || in.input.isStackPtr()) {
        inout.output = DynamicValue(DynamicValueType::Tainted);
    } else {
        inout.output = DynamicValue(DynamicValueType::Unknown);
    }
    return EmuRet::Ok;
}

static inline __uint128_t addpd(__uint128_t a,  __uint128_t b)
{
    asm volatile ("     addpd %[in], %[inout]\n"
                  : [inout] "+x" (a)
                  : [in] "x" (b)
                  : "memory" );
    return a;
}

DEF_EMULATE_FN(addpd)
{
    DynamicOperandInfo &inout = dynInfo.operands[0];
    DynamicOperandInfo &in = dynInfo.operands[1];

    inout.output = addpd(inout.input.getImm128(), in.input.getImm128());
    return EmuRet::Ok;
}

static inline uint64_t addsd(uint64_t a, uint64_t b)
{
    asm volatile ("     addsd %[in], %[inout]\n"
                  : [inout] "+x" (a)
                  : [in] "x" (b)
                  : "memory" );
    return a;
}

DEF_EMULATE_FN(addsd)
{
    DynamicOperandInfo &inout = dynInfo.operands[0];
    DynamicOperandInfo &in = dynInfo.operands[1];

    inout.output = addsd(inout.input.getImm64(), in.input.getImm64());
    return EmuRet::Ok;
}

DEF_EMULATE_FN(call)
{
    DynamicOperandInfo &rsp = dynInfo.operands[1];

    if (rsp.input.isImm()) {
        rsp.output = rsp.input.getImm64() - 8;
    } else if (rsp.input.isPtr()) {
        rsp.output = DynamicValue(rsp.input.getType(), rsp.input.getNr(),
                          rsp.input.getPtrOffset() - 8);
    } else {
        rsp.output = rsp.input;
    }

    /*
     * For now, always use number 1, that at least helps to differentiate from
     * level 0 (entry function).
     */
    dynInfo.operands[2].output = DynamicValue(DynamicValueType::ReturnPtr, 1, 0);
    return EmuRet::Ok;
}

DEF_EMULATE_FN(lea)
{
    dynInfo.operands[0].output = dynInfo.operands[1].input;
    return EmuRet::Ok;
}

DEF_EMULATE_FN(mov)
{
    dynInfo.operands[0].output = dynInfo.operands[1].input;
    return EmuRet::Mov10;
}

static inline void cmp8(uint8_t a, uint8_t b, Eflags &eflags)
{
    asm volatile ("     cmpb %[in1], %[in0]\n"
                  :
                  : [in0] "g" (a),
                    [in1] "r" (b)
                  : "memory" );
    readEflags(eflags);
}

DEF_EMULATE_FN(cmp8)
{
    DynamicOperandInfo &in0 = dynInfo.operands[0];
    DynamicOperandInfo &in1 = dynInfo.operands[1];
    Eflags eflags;

    cmp8(in0.input.getImm64(), in1.input.getImm64(), eflags);
    setEflags(dynInfo, 2, eflags);
    return EmuRet::Ok;
}

static inline void cmp16(uint16_t a, uint16_t b, Eflags &eflags)
{
    asm volatile ("     cmpw %[in1], %[in0]\n"
                  :
                  : [in0] "g" (a),
                    [in1] "r" (b)
                  : "memory" );
    readEflags(eflags);
}

DEF_EMULATE_FN(cmp16)
{
    DynamicOperandInfo &in0 = dynInfo.operands[0];
    DynamicOperandInfo &in1 = dynInfo.operands[1];
    Eflags eflags;

    cmp16(in0.input.getImm64(), in1.input.getImm64(), eflags);
    setEflags(dynInfo, 2, eflags);
    return EmuRet::Ok;
}

static inline void cmp32(uint32_t a, uint32_t b, Eflags &eflags)
{
    asm volatile ("     cmpl %[in1], %[in0]\n"
                  :
                  : [in0] "g" (a),
                    [in1] "r" (b)
                  : "memory" );
    readEflags(eflags);
}

DEF_EMULATE_FN(cmp32)
{
    DynamicOperandInfo &in0 = dynInfo.operands[0];
    DynamicOperandInfo &in1 = dynInfo.operands[1];
    Eflags eflags;

    cmp32(in0.input.getImm64(), in1.input.getImm64(), eflags);
    setEflags(dynInfo, 2, eflags);
    return EmuRet::Ok;
}

static inline void cmp64(uint64_t a, uint64_t b, Eflags &eflags)
{
    asm volatile ("     cmpq %[in1], %[in0]\n"
                  :
                  : [in0] "g" (a),
                    [in1] "r" (b)
                  : "memory" );
    readEflags(eflags);
}

DEF_EMULATE_FN(cmp64)
{
    DynamicOperandInfo &in0 = dynInfo.operands[0];
    DynamicOperandInfo &in1 = dynInfo.operands[1];
    Eflags eflags;

    if (in0.input.isImm() && in1.input.isImm()) {
        cmp64(in0.input.getImm64(), in1.input.getImm64(), eflags);
        setEflags(dynInfo, 2, eflags);
    } else if (in0.input.isPtr() && in1.input.isPtr() &&
               in0.input.getType() == in1.input.getType() &&
               in0.input.getNr() == in1.input.getNr()) {
        // FIXME will this result in the right values?
        cmp64(in0.input.getPtrOffset(), in1.input.getPtrOffset(), eflags);
        setEflags(dynInfo, 2, eflags);
    }
    /*
     * TODO: we could make use of UsrPtr information here, e.g. comparing a
     * pointer against immediates/zero
     */
    return EmuRet::Ok;
}

static inline __uint128_t mulpd(__uint128_t a,  __uint128_t b)
{
    asm volatile ("     mulpd %[in], %[inout]\n"
                  : [inout] "+x" (a)
                  : [in] "x" (b)
                  : "memory" );
    return a;
}

DEF_EMULATE_FN(mulpd)
{
    DynamicOperandInfo &inout = dynInfo.operands[0];
    DynamicOperandInfo &in = dynInfo.operands[1];

    inout.output = mulpd(inout.input.getImm128(), in.input.getImm128());
    return EmuRet::Ok;
}

static inline uint64_t mulsd(uint64_t a, uint64_t b)
{
    asm volatile ("     mulsd %[in], %[inout]\n"
                  : [inout] "+x" (a)
                  : [in] "x" (b)
                  : "memory" );
    return a;
}

DEF_EMULATE_FN(mulsd)
{
    DynamicOperandInfo &inout = dynInfo.operands[0];
    DynamicOperandInfo &in = dynInfo.operands[1];

    inout.output = mulsd(inout.input.getImm64(), in.input.getImm64());
    return EmuRet::Ok;
}

DEF_EMULATE_FN(pop)
{
    const uint64_t size = static_cast<uint8_t>(dynInfo.operands[2].memAcc.size);
    DynamicOperandInfo &rsp = dynInfo.operands[1];

    if (rsp.input.isImm()) {
        rsp.output = rsp.input.getImm64() + size;
    } else if (rsp.input.isPtr()) {
        rsp.output = DynamicValue(rsp.input.getType(), rsp.input.getNr(),
                          rsp.input.getPtrOffset() + size);
    } else {
        rsp.output = rsp.input;
    }

    dynInfo.operands[0].output = dynInfo.operands[2].input;
    return EmuRet::Mov20;
}

DEF_EMULATE_FN(push)
{
    const uint64_t size = static_cast<uint8_t>(dynInfo.operands[2].memAcc.size);
    DynamicOperandInfo &rsp = dynInfo.operands[1];

    if (rsp.input.isImm()) {
        rsp.output = rsp.input.getImm64() - size;
    } else if (rsp.input.isPtr()) {
        rsp.output = DynamicValue(rsp.input.getType(), rsp.input.getNr(),
                          rsp.input.getPtrOffset() - size);
    } else {
        rsp.output = rsp.input;
    }

    dynInfo.operands[2].output = dynInfo.operands[0].input;
    return EmuRet::Mov02;
}

static inline __uint128_t pxor(__uint128_t a,  __uint128_t b)
{
    asm volatile ("     pxor %[in], %[inout]\n"
                  : [inout] "+x" (a)
                  : [in] "x" (b)
                  : "memory" );
    return a;
}

DEF_EMULATE_FN(pxor)
{
    DynamicOperandInfo &inout = dynInfo.operands[0];
    DynamicOperandInfo &in = dynInfo.operands[1];

    inout.output = pxor(inout.input.getImm128(), in.input.getImm128());
    return EmuRet::Ok;
}

DEF_EMULATE_FN(ret)
{
    DynamicOperandInfo &rsp = dynInfo.operands[0];

    if (rsp.input.isImm()) {
        rsp.output = rsp.input.getImm64() + 8;
    } else if (rsp.input.isPtr()) {
        rsp.output = DynamicValue(rsp.input.getType(), rsp.input.getNr(),
                          rsp.input.getPtrOffset() + 8);
    } else {
        rsp.output = rsp.input;
    }
    return EmuRet::Ok;
}

static uint64_t shl64(uint64_t a, uint8_t shift, Eflags &eflags)
{
    asm volatile ("     shlq %[shift], %[inout]\n"
                  : [inout] "+r" (a)
                  : [shift] "c" (shift)
                  : "memory" );
    readEflags(eflags);

    return a;
}

DEF_EMULATE_FN(shl64)
{
    DynamicOperandInfo &inout = dynInfo.operands[0];
    DynamicOperandInfo &shift = dynInfo.operands[1];
    Eflags eflags;

    if (inout.input.isImm() && shift.input.isImm()) {
        inout.output = shl64(inout.input.getImm64(), shift.input.getImm64(), eflags);
        setEflags(dynInfo, 2, eflags);
    } else if (inout.input.isStackPtr()) {
        inout.output = DynamicValue(DynamicValueType::Tainted);
    } else if (inout.input.isPtr()) {
        inout.output = DynamicValue(DynamicValueType::Unknown);
    } else {
        inout.output = inout.input;
    }
    /*
     * TODO: we could make use of UsrPtr information here, e.g. shifting the
     * actual pointer value.
     */
    if (!shift.input.isImm() || shift.input.getImm64() != 1) {
        /* OF is undefined when not a 1-bit shift */
        setOF(dynInfo, 7, DynamicValue(DynamicValueType::Unknown));
    }
    if (!shift.input.isImm() || shift.input.getImm64() >= 64) {
        /* CF is undefined when count is greater or equal to size in bits */
        setCF(dynInfo, 2, DynamicValue(DynamicValueType::Unknown));
    }
    /* AF is undefined  */
    setAF(dynInfo, 4, DynamicValue(DynamicValueType::Unknown));

    return EmuRet::Ok;
}

static uint64_t shr64(uint64_t a, uint8_t shift, Eflags &eflags)
{
    asm volatile ("     shrq %[shift], %[inout]\n"
                  : [inout] "+r" (a)
                  : [shift] "c" (shift)
                  : "memory" );
    readEflags(eflags);

    return a;
}

DEF_EMULATE_FN(shr64)
{
    DynamicOperandInfo &inout = dynInfo.operands[0];
    DynamicOperandInfo &shift = dynInfo.operands[1];
    Eflags eflags;

    if (inout.input.isImm() && shift.input.isImm()) {
        inout.output = shr64(inout.input.getImm64(), shift.input.getImm64(), eflags);
        setEflags(dynInfo, 2, eflags);
    } else if (inout.input.isStackPtr()) {
        inout.output = DynamicValue(DynamicValueType::Tainted);
    } else if (inout.input.isPtr()) {
        inout.output = DynamicValue(DynamicValueType::Unknown);
    } else {
        inout.output = inout.input;
    }
    /*
     * TODO: we could make use of UsrPtr information here, e.g. shifting the
     * actual pointer value.
     */
    if (!shift.input.isImm() || shift.input.getImm64() != 1) {
        /* OF is undefined when not a 1-bit shift */
        setOF(dynInfo, 7, DynamicValue(DynamicValueType::Unknown));
    }
    if (!shift.input.isImm() || shift.input.getImm64() >= 64) {
        /* CF is undefined when count is greater or equal to size in bits */
        setCF(dynInfo, 2, DynamicValue(DynamicValueType::Unknown));
    }
    /* AF is undefined  */
    setAF(dynInfo, 4, DynamicValue(DynamicValueType::Unknown));

    return EmuRet::Ok;
}

#define GEN_SUB(BITS, C) \
static inline uint##BITS##_t sub##BITS(uint##BITS##_t a, uint##BITS##_t b, \
                                       Eflags &eflags) \
{ \
    asm volatile ("     sub" #C " %[in], %[inout]\n" \
                  : [inout] "+g" (a) \
                  : [in] "r" (b) \
                  : "memory" ); \
    readEflags(eflags); \
    return a; \
}
GEN_SUB(8, b)
GEN_SUB(16, w)
GEN_SUB(32, l)
GEN_SUB(64, q)

#define GEN_EMULATE_SUB(BITS) \
DEF_EMULATE_FN(sub##BITS) \
{ \
    DynamicOperandInfo &inout = dynInfo.operands[0]; \
    DynamicOperandInfo &in = dynInfo.operands[1]; \
    Eflags eflags; \
\
    inout.output = sub##BITS(inout.input.getImm64(), in.input.getImm64(), eflags);\
    setEflags(dynInfo, 2, eflags); \
    return EmuRet::Ok;\
}
GEN_EMULATE_SUB(8)
GEN_EMULATE_SUB(16)
GEN_EMULATE_SUB(32)

DEF_EMULATE_FN(sub64)
{
    DynamicOperandInfo &inout = dynInfo.operands[0];
    DynamicOperandInfo &in = dynInfo.operands[1];
    Eflags eflags;

    /* Outputs (and therefore flags) are marked as unknown as default */
    if (inout.input.isImm() && in.input.isImm()) {
        inout.output = sub64(inout.input.getImm64(), in.input.getImm64(), eflags);
        setEflags(dynInfo, 2, eflags);
    } else if (inout.input.isPtr() && in.input.isImm()) {
        int64_t offs = sub64(inout.input.getPtrOffset(), in.input.getImm64(), eflags);

        inout.output = DynamicValue(inout.input.getType(), inout.input.getNr(), offs);
    } else if (in.input.isPtr() && inout.input.isImm()) {
        int64_t offs = sub64(in.input.getPtrOffset(), inout.input.getImm64(), eflags);

        inout.output = DynamicValue(in.input.getType(), in.input.getNr(), offs);
    } else if (inout.input.isStackPtr() || in.input.isStackPtr()) {
        inout.output = DynamicValue(DynamicValueType::Tainted);
    } else {
        inout.output = DynamicValue(DynamicValueType::Unknown);
    }
    return EmuRet::Ok;
}

static inline void test8(uint8_t a, uint8_t b, Eflags &eflags)
{
    asm volatile ("     testb %[in1], %[in0]\n"
                  :
                  : [in0] "r" (a),
                    [in1] "r" (b)
                  : "memory" );
    readEflags(eflags);
}

DEF_EMULATE_FN(test8)
{
    DynamicOperandInfo &in0 = dynInfo.operands[0];
    DynamicOperandInfo &in1 = dynInfo.operands[1];
    Eflags eflags;

    if (in0.input.isImm() && in1.input.isImm()) {
        test8(in0.input.getImm64(), in1.input.getImm64(), eflags);
        setEflags(dynInfo, 2, eflags);
    }
    /* the following flags don't depend on any input values */
    setCF(dynInfo, 2, DynamicValue((uint8_t)0));
    setAF(dynInfo, 4, DynamicValue(DynamicValueType::Unknown));
    setOF(dynInfo, 7, DynamicValue((uint8_t)0));
    return EmuRet::Ok;
}

static inline void test16(uint16_t a, uint16_t b, Eflags &eflags)
{
    asm volatile ("     testw %[in1], %[in0]\n"
                  :
                  : [in0] "r" (a),
                    [in1] "r" (b)
                  : "memory" );
    readEflags(eflags);
}

DEF_EMULATE_FN(test16)
{
    DynamicOperandInfo &in0 = dynInfo.operands[0];
    DynamicOperandInfo &in1 = dynInfo.operands[1];
    Eflags eflags;

    if (in0.input.isImm() && in1.input.isImm()) {
        test16(in0.input.getImm64(), in1.input.getImm64(), eflags);
        setEflags(dynInfo, 2, eflags);
    }
    /* the following flags don't depend on any input values */
    setCF(dynInfo, 2, DynamicValue((uint8_t)0));
    setAF(dynInfo, 4, DynamicValue(DynamicValueType::Unknown));
    setOF(dynInfo, 7, DynamicValue((uint8_t)0));
    return EmuRet::Ok;
}

static inline void test32(uint32_t a, uint32_t b, Eflags &eflags)
{
    asm volatile ("     testl %[in1], %[in0]\n"
                  :
                  : [in0] "r" (a),
                    [in1] "r" (b)
                  : "memory" );
    readEflags(eflags);
}

DEF_EMULATE_FN(test32)
{
    DynamicOperandInfo &in0 = dynInfo.operands[0];
    DynamicOperandInfo &in1 = dynInfo.operands[1];
    Eflags eflags;

    if (in0.input.isImm() && in1.input.isImm()) {
        test32(in0.input.getImm64(), in1.input.getImm64(), eflags);
        setEflags(dynInfo, 2, eflags);
    }
    /* the following flags don't depend on any input values */
    setCF(dynInfo, 2, DynamicValue((uint8_t)0));
    setAF(dynInfo, 4, DynamicValue(DynamicValueType::Unknown));
    setOF(dynInfo, 7, DynamicValue((uint8_t)0));
    return EmuRet::Ok;
}

static inline void test64(uint64_t a, uint64_t b, Eflags &eflags)
{
    asm volatile ("     testq %[in1], %[in0]\n"
                  :
                  : [in0] "r" (a),
                    [in1] "r" (b)
                  : "memory" );
    readEflags(eflags);
}

DEF_EMULATE_FN(test64)
{
    DynamicOperandInfo &in0 = dynInfo.operands[0];
    DynamicOperandInfo &in1 = dynInfo.operands[1];
    Eflags eflags;

    if (in0.input.isImm() && in1.input.isImm()) {
        test64(in0.input.getImm64(), in1.input.getImm64(), eflags);
        setEflags(dynInfo, 2, eflags);
    }
    /* the following flags don't depend on any input values */
    setCF(dynInfo, 2, DynamicValue((uint8_t)0));
    setAF(dynInfo, 4, DynamicValue(DynamicValueType::Unknown));
    setOF(dynInfo, 7, DynamicValue((uint8_t)0));
    /*
     * TODO: we could make use of UsrPtr information here, e.g. comparing a
     * pointer against immediates/zero
     */
    return EmuRet::Ok;
}

static inline uint32_t xor32(uint32_t a, uint32_t b, Eflags &eflags)
{
    asm volatile ("     xorl %[in], %[inout]\n"
                  : [inout] "+g" (a)
                  : [in] "r" (b)
                  : "memory" );
    readEflags(eflags);
    return a;
}

DEF_EMULATE_FN(xor32)
{
    DynamicOperandInfo &inout = dynInfo.operands[0];
    DynamicOperandInfo &in = dynInfo.operands[1];
    Eflags eflags;

    inout.output = xor32(inout.input.getImm64(), in.input.getImm64(), eflags);
    setEflags(dynInfo, 2, eflags);
    return EmuRet::Ok;
}

static inline uint64_t xor64(uint64_t a, uint64_t b, Eflags &eflags)
{
    asm volatile ("     xorq %[in], %[inout]\n"
                  : [inout] "+g" (a)
                  : [in] "r" (b)
                  : "memory" );
    readEflags(eflags);
    return a;
}

DEF_EMULATE_FN(xor64)
{
    DynamicOperandInfo &inout = dynInfo.operands[0];
    DynamicOperandInfo &in = dynInfo.operands[1];
    Eflags eflags;

    inout.output = xor64(inout.input.getImm64(), in.input.getImm64(), eflags);
    setEflags(dynInfo, 2, eflags);
    return EmuRet::Ok;
}

} /* namespace drob */
