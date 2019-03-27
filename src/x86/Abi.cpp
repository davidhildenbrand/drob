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
#include "arch.hpp"
#include "drob_internal.h"
#include "../Rewriter.hpp"

namespace drob {

/*
 * While the AMD System V ABI talks about different categories
 * (INTEGER, SSE, SSEUP), which is sufficient for allocation in e.g.
 * 8 byte blocks, the actually unused portions of such an block are in
 * general undefined. We want to model that.
 *
 * E.g. BOOL is returned in RAX. However, only the lowest byte (AL) is
 * defined. In the other 7 bytes, we could have garbage. Same applies
 * to parameters passed on the stack or in registers. 8 bytes might get
 * reserved on the stack, but only some portion is actually defined.
 *
 * See
 * * https://stackoverflow.com/questions/36706721/is-a-sign-or-zero-extension-required-when-adding-a-32bit-offset-to-a-pointer-for/36760539#36760539
 * * https://github.com/hjl-tools/x86-psABI/issues/2
 *
 * CLANG:
 * * "Return values have garbage outside the width of the return type."
 * * "narrow types are sign or zero extended to 32 bits, depending on their
 *    type. clang depends on this for incoming args, but gcc doesn't make
 *    that assumption. But both compilers do it when calling, so gcc code
 *    can call clang code."
 * * "The upper 32 bits of registers are always undefined garbage for types
 *    smaller than 64 bits."
 *
 * Both, GCC and CLANG handle it that way, so will we, although it it
 * not oficially part of the ABI.
 */
typedef enum class AMD64Class {
    NONE,
    INTEGER8,
    INTEGER16,
    INTEGER32,
    INTEGER64,
    INTEGER128,
    SSE32, /* 4 bytes only */
    SSE64, /* 8 bytes only */
    SSE128, /* SSE + SSEUP (16 bytes -> one xmm register) */
} AMD64Class;

/*
 * For now, no aggregates (structures and arrays) are supported as specification
 * on the inteface would be highly weird. (especially as the rules are
 * less trivial). User can use wrapper functions or pointers.
 *
 * Also not supported are
 * - vararg
 * - __m64
 * - long double (X87)
 * - __m256 (AVX256)
 */

static AMD64Class drobParamTypeToAMD64(drob_param_type type)
{
    switch(type) {
    case DROB_PARAM_TYPE_VOID:
        return AMD64Class::NONE;
    case DROB_PARAM_TYPE_BOOL:
    case DROB_PARAM_TYPE_CHAR:
    case DROB_PARAM_TYPE_UCHAR:
    case DROB_PARAM_TYPE_INT8:
    case DROB_PARAM_TYPE_UINT8:
        return AMD64Class::INTEGER8;
    case DROB_PARAM_TYPE_SHORT:
    case DROB_PARAM_TYPE_USHORT:
    case DROB_PARAM_TYPE_INT16:
    case DROB_PARAM_TYPE_UINT16:
        return AMD64Class::INTEGER16;
    case DROB_PARAM_TYPE_INT:
    case DROB_PARAM_TYPE_UINT:
    case DROB_PARAM_TYPE_UINT32:
    case DROB_PARAM_TYPE_INT32:
        return AMD64Class::INTEGER32;
    case DROB_PARAM_TYPE_LONG:
    case DROB_PARAM_TYPE_ULONG:
    case DROB_PARAM_TYPE_LONGLONG:
    case DROB_PARAM_TYPE_ULONGLONG:
    case DROB_PARAM_TYPE_INT64:
    case DROB_PARAM_TYPE_UINT64:
    case DROB_PARAM_TYPE_PTR:
        return AMD64Class::INTEGER64;
    case DROB_PARAM_TYPE_INT128:
    case DROB_PARAM_TYPE_UINT128:
        return AMD64Class::INTEGER128;
    case DROB_PARAM_TYPE_FLOAT:
        return AMD64Class::SSE32;
    case DROB_PARAM_TYPE_DOUBLE:
        return AMD64Class::SSE64;
    case DROB_PARAM_TYPE_M128:
    case DROB_PARAM_TYPE_FLOAT128:
        return AMD64Class::SSE128;
    default:
        drob_assert_not_reached();
    }
}

static void translateRet(const drob_cfg &cfg, FunctionSpecification *entrySpec,
             ProgramState *entryState)
{
    (void)entryState;

    /*
     * The stack always points at the 8-byte return value we will read
     * with the ret instruction.
     */
    entrySpec->stack.in.push_back(StackRange({0, 8}));
    entryState->setStack(0, MemAccessSize::B8, Data(DataType::ReturnPtr, 0, 0));

    AMD64Class ret = drobParamTypeToAMD64(cfg.ret_type);
    switch (ret) {
    case AMD64Class::NONE:
        break;
    case AMD64Class::INTEGER8:
        entrySpec->reg.out += getSubRegisterMask(Register::AL);
        break;
    case AMD64Class::INTEGER16:
        entrySpec->reg.out += getSubRegisterMask(Register::AX);
        break;
    case AMD64Class::INTEGER32:
        entrySpec->reg.out += getSubRegisterMask(Register::EAX);
        break;
    case AMD64Class::INTEGER64:
        entrySpec->reg.out += getSubRegisterMask(Register::RAX);
        break;
    case AMD64Class::INTEGER128:
        entrySpec->reg.out += getSubRegisterMask(Register::RDX);
        entrySpec->reg.out += getSubRegisterMask(Register::RAX);
        break;
    case AMD64Class::SSE32:
        entrySpec->reg.out += getSubRegisterMask(Register::XMM0,
                             RegisterAccessType::F0);
        break;
    case AMD64Class::SSE64:
        entrySpec->reg.out += getSubRegisterMask(Register::XMM0,
                             RegisterAccessType::H0);
        break;
    case AMD64Class::SSE128:
        entrySpec->reg.out += getSubRegisterMask(Register::XMM0);
        break;
    default:
        drob_assert_not_reached();
    };
}

/*
 * Sign-extend where necessary. (see comment regarding clang and GCC ABI
 * comments)
 */
static uint32_t drobParamToImm32(const drob_param_cfg &param)
{
    drob_assert(param.state == DROB_PARAM_STATE_CONST);

    switch(param.type) {
    case DROB_PARAM_TYPE_BOOL:
        return (uint32_t)param.value.bool_val;
    case DROB_PARAM_TYPE_CHAR:
        return (int32_t)param.value.char_val;
    case DROB_PARAM_TYPE_UCHAR:
        return (uint32_t)param.value.uchar_val;
    case DROB_PARAM_TYPE_INT8:
        return (int32_t)param.value.int8_val;
    case DROB_PARAM_TYPE_UINT8:
        return (uint32_t)param.value.uint8_val;
    case DROB_PARAM_TYPE_SHORT:
        return (int32_t)param.value.short_val;
    case DROB_PARAM_TYPE_USHORT:
        return (uint32_t)param.value.ushort_val;
    case DROB_PARAM_TYPE_INT16:
        return (int32_t)param.value.int16_val;
    case DROB_PARAM_TYPE_UINT16:
        return (uint32_t)param.value.uint16_val;
    case DROB_PARAM_TYPE_INT:
        return param.value.int_val;
    case DROB_PARAM_TYPE_UINT:
        return param.value.uint_val;
    case DROB_PARAM_TYPE_INT32:
        return param.value.int32_val;
    case DROB_PARAM_TYPE_UINT32:
        return param.value.uint32_val;
    default:
        drob_assert_not_reached();
    }
}

static const Register integer64_regs[6] {
    Register::RDI,
    Register::RSI,
    Register::RDX,
    Register::RCX,
    Register::R8,
    Register::R9,
};

static const Register integer32_regs[6] {
    Register::EDI,
    Register::ESI,
    Register::EDX,
    Register::ECX,
    Register::R8D,
    Register::R9D,
};

static const Register sse_regs[8] {
    Register::XMM0,
    Register::XMM1,
    Register::XMM2,
    Register::XMM3,
    Register::XMM4,
    Register::XMM5,
    Register::XMM6,
    Register::XMM7,
};

static void translateParam(const drob_param_cfg &param, RewriterCfg &cfg,
               unsigned int *intIdx, unsigned int *sseIdx,
               int *stackOffset)
{
    FunctionSpecification *entrySpec = &cfg.getEntrySpec();
    ProgramState *entryState = &cfg.getEntryState();

    AMD64Class type = drobParamTypeToAMD64(param.type);

    switch (type) {
    case AMD64Class::INTEGER8:
    case AMD64Class::INTEGER16:
    case AMD64Class::INTEGER32:
        if (*intIdx < sizeof(integer32_regs)) {
            entrySpec->reg.in += getSubRegisterMask(integer32_regs[*intIdx]);
            if (param.state == DROB_PARAM_STATE_CONST)
                entryState->setRegister(integer32_regs[*intIdx],
                            drobParamToImm32(param));
            else
                entryState->setRegister(integer32_regs[*intIdx],
                            DataType::Unknown);
            (*intIdx)++;
        } else {
            entrySpec->stack.in.push_back(StackRange( { *stackOffset, 4 } ));
            if (param.state == DROB_PARAM_STATE_CONST)
                entryState->setStack(*stackOffset, MemAccessSize::B4,
                             drobParamToImm32(param));
            else
                entryState->setStack(*stackOffset, MemAccessSize::B4,
                             DataType::Unknown);
            *stackOffset += 8;
        }
        break;
    case AMD64Class::INTEGER64:
        if (*intIdx < sizeof(integer64_regs)) {
            entrySpec->reg.in += getSubRegisterMask(integer64_regs[*intIdx]);
            if (param.state == DROB_PARAM_STATE_CONST)
                entryState->setRegister(integer64_regs[*intIdx],
                            param.value.uint64_val);
            else
                entryState->setRegister(integer64_regs[*intIdx],
                            DataType::Unknown);
            (*intIdx)++;
        } else {
            entrySpec->stack.in.push_back(StackRange( { *stackOffset, 8 } ));
            if (param.state == DROB_PARAM_STATE_CONST)
                entryState->setStack(*stackOffset, MemAccessSize::B8,
                             param.value.uint64_val);
            else
                entryState->setStack(*stackOffset, MemAccessSize::B8,
                             DataType::Unknown);
            *stackOffset += 8;
        }
        break;
    case AMD64Class::INTEGER128: {
        union {
            unsigned __int128 i128;
            uint64_t i[2]; /* low: i[0], high: i[1] */
        } val;

        /* signed/unsigned should not mather here */
        val.i128 = param.value.uint128_val;

        /* we will need two general purpose register */
        if ((*intIdx) + 1 < sizeof(integer64_regs)) {
            entrySpec->reg.in += getSubRegisterMask(integer64_regs[*intIdx]);
            if (param.state == DROB_PARAM_STATE_CONST)
                entryState->setRegister(integer64_regs[*intIdx],
                            val.i[0]);
            else
                entryState->setRegister(integer64_regs[*intIdx],
                            DataType::Unknown);
            (*intIdx)++;
            entrySpec->reg.in += getSubRegisterMask(integer64_regs[*intIdx]);
            if (param.state == DROB_PARAM_STATE_CONST)
                entryState->setRegister(integer64_regs[*intIdx],
                            val.i[1]);
            else
                entryState->setRegister(integer64_regs[*intIdx],
                            DataType::Unknown);
            (*intIdx)++;
        } else {
            /*
             * Has to be aligned to 16 bytes, I assume this is
             * passed on the stack and not in memory.
             */
            if (*stackOffset % 16) {
                (*stackOffset) += 8;
                drob_assert(!(*stackOffset % 16));
            }
            entrySpec->stack.in.push_back(StackRange( { *stackOffset, 16 } ));
            if (param.state == DROB_PARAM_STATE_CONST)
                entryState->setStack(*stackOffset, MemAccessSize::B16,
                             param.value.uint128_val);
            else
                entryState->setStack(*stackOffset, MemAccessSize::B16,
                             DataType::Unknown);
            *stackOffset += 16;
        }
        break;
    }
    case AMD64Class::SSE32: {
        if (*sseIdx < sizeof(sse_regs)) {
            entrySpec->reg.in += getSubRegisterMask(sse_regs[*sseIdx],
                                RegisterAccessType::F0);
            if (param.state == DROB_PARAM_STATE_CONST)
                entryState->setRegister(sse_regs[*sseIdx],
                            RegisterAccessType::F0,
                            param.value.uint32_val);
            else
                entryState->setRegister(sse_regs[*sseIdx],
                            RegisterAccessType::F0,
                            DataType::Unknown);
            (*sseIdx)++;
        } else {
            entrySpec->stack.in.push_back(StackRange( { *stackOffset, 4 } ));
            if (param.state == DROB_PARAM_STATE_CONST)
                entryState->setStack(*stackOffset, MemAccessSize::B4,
                             param.value.uint32_val);
            else
                entryState->setStack(*stackOffset, MemAccessSize::B4,
                             DataType::Unknown);
            *stackOffset += 8;
        }
        break;
    }
    case AMD64Class::SSE64:
        if (*sseIdx < sizeof(sse_regs)) {
            entrySpec->reg.in += getSubRegisterMask(sse_regs[*sseIdx],
                                RegisterAccessType::H0);
            if (param.state == DROB_PARAM_STATE_CONST)
                entryState->setRegister(sse_regs[*sseIdx],
                            RegisterAccessType::H0,
                            param.value.uint64_val);
            else
                entryState->setRegister(sse_regs[*sseIdx],
                            RegisterAccessType::H0,
                            DataType::Unknown);
            (*sseIdx)++;
        } else {
            entrySpec->stack.in.push_back(StackRange( { *stackOffset, 8 } ));
            if (param.state == DROB_PARAM_STATE_CONST)
                entryState->setStack(*stackOffset, MemAccessSize::B8,
                             param.value.uint64_val);
            else
                entryState->setStack(*stackOffset, MemAccessSize::B8,
                             DataType::Unknown);
            *stackOffset += 8;
        }
        break;
    case AMD64Class::SSE128:
        if (*sseIdx < sizeof(sse_regs)) {
            entrySpec->reg.in += getSubRegisterMask(sse_regs[*sseIdx]);
            if (param.state == DROB_PARAM_STATE_CONST)
                entryState->setRegister(sse_regs[*sseIdx],
                            param.value.uint128_val);
            else
                entryState->setRegister(sse_regs[*sseIdx],
                            DataType::Unknown);
            (*sseIdx)++;
        } else {
            entrySpec->stack.in.push_back(StackRange( { *stackOffset, 16 } ));
            if (param.state == DROB_PARAM_STATE_CONST)
                entryState->setStack(*stackOffset, MemAccessSize::B16,
                             param.value.uint128_val);
            else
                entryState->setStack(*stackOffset, MemAccessSize::B16,
                             DataType::Unknown);
            *stackOffset += 16;
        }
        break;
    case AMD64Class::NONE:
        drob_throw("void not valid for parameter type");
    default:
        drob_assert_not_reached();
    };
}

static void translatePtr(const drob_param_cfg &param, RewriterCfg &cfg,
             unsigned int *intIdx, int *stackOffset)
{
    FunctionSpecification &entrySpec = cfg.getEntrySpec();
    ProgramState &entryState = cfg.getEntryState();
    unsigned int nr = cfg.nextUsrPtr();
    UsrPtrCfg &ptrCfg = cfg.getUsrPtrCfg(nr);

    if (*intIdx < sizeof(integer64_regs)) {
        entrySpec.reg.in += getSubRegisterMask(integer64_regs[*intIdx]);
        entryState.setRegister(integer64_regs[*intIdx],
                       Data(DataType::UsrPtr, nr, 0));
        (*intIdx)++;
    } else {
        entrySpec.stack.in.push_back(StackRange( { *stackOffset, 8 } ));
        entryState.setStack(*stackOffset, MemAccessSize::B8,
                    Data(DataType::UsrPtr, nr, 0));
        *stackOffset += 8;
    }

    if (param.state == DROB_PARAM_STATE_CONST) {
        ptrCfg.isKnown = true;
        ptrCfg.val = param.value.ptr_val;
    }
    if (param.ptr_flags & 1ULL << DROB_PTR_FLAG_CONST)
        ptrCfg.isConst = true;
    if (param.ptr_flags & 1ULL << DROB_PTR_FLAG_RESTRICT)
        ptrCfg.isRestrict = true;
    if (param.ptr_flags & 1ULL << DROB_PTR_FLAG_NOTNULL)
        ptrCfg.isNotNull = true;
    ptrCfg.align = param.ptr_align;
}

/*
 * On Linux, AMD64 System V ABI applies to our entry function.
 */
void arch_translate_cfg(const drob_cfg &drob_cfg, RewriterCfg &cfg)
{
    FunctionSpecification *entrySpec = &cfg.getEntrySpec();
    ProgramState *entryState = &cfg.getEntryState();
    unsigned int intIdx = 0, sseIdx = 0;
    int i, stackOffset = 8; /* stack offset above the ReturnIP */

    entrySpec->reg.in.zero();
    entrySpec->reg.out.zero();
    entrySpec->reg.preserved.zero();

    /* translate the return type and the return address on the stack */
    translateRet(drob_cfg, entrySpec, entryState);

    /* translate all parameters */
    for (i = 0; i < drob_cfg.param_count; i++) {
        const drob_param_cfg param = drob_cfg.params[i];

        if (param.type == DROB_PARAM_TYPE_PTR)
            translatePtr(param, cfg, &intIdx, &stackOffset);
        else
            translateParam(param, cfg, &intIdx, &sseIdx, &stackOffset);
    }

    /* handle the stack pointer passed via RSP */
    entrySpec->reg.in += getSubRegisterMask(Register::RSP);
    entrySpec->reg.preserved += getSubRegisterMask(Register::RSP);
    entryState->setRegister(Register::RSP, Data(DataType::StackPtr, 0, 0));

    /* mark callee-saved registers */
    entrySpec->reg.preserved += getSubRegisterMask(Register::RBX);
    entryState->setRegister(Register::RBX, Data(DataType::Preserved8, 0));
    entrySpec->reg.preserved += getSubRegisterMask(Register::RBP);
    entryState->setRegister(Register::RBP, Data(DataType::Preserved8, 1));
    entrySpec->reg.preserved += getSubRegisterMask(Register::R12);
    entryState->setRegister(Register::R12, Data(DataType::Preserved8, 2));
    entrySpec->reg.preserved += getSubRegisterMask(Register::R13);
    entryState->setRegister(Register::R13, Data(DataType::Preserved8, 3));
    entrySpec->reg.preserved += getSubRegisterMask(Register::R14);
    entryState->setRegister(Register::R14, Data(DataType::Preserved8, 4));
    entrySpec->reg.preserved += getSubRegisterMask(Register::R15);
    entryState->setRegister(Register::R15, Data(DataType::Preserved8, 5));
    /* fixme: direction flag, mxcsr */

    if (unlikely(loglevel >= DROB_LOGLEVEL_DEBUG)) {
        drob_dump("Input registers");
        entrySpec->reg.in.dump();
        drob_dump("Output registers");
        entrySpec->reg.out.dump();
        drob_dump("Preserved registers");
        entrySpec->reg.preserved.dump();
    }
}

} /* namespace drob */
