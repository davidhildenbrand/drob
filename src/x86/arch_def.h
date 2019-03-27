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
#ifndef ARCH_DEF_H
#define ARCH_DEF_H

namespace drob {

/* Maximum displacement for RIP relative addressing is 32bit signed */
#define ARCH_MAX_MMAP_SIZE (1ull << 31)
/* Align to 16 bytes, recommended by Intel */
#define ARCH_BLOCK_ALIGN 16
#define ARCH_PAGE_SIZE 4096
#define ARCH_MAX_ILEN 15
/* No AVX/VEX support yet. E.g. IMUL has a version with three operands */
#define ARCH_MAX_OPERANDS 3

#define ARCH_FLAG1_COUNT 6
#define ARCH_GPRS64_COUNT 16
#define ARCH_SSE128_COUNT 16

/*
 * Addressing mode: Can be concluded from the used memory offset /
 * used registers in indirect addressing mode.
 * - 32-bit effective addresses: Base/Index/EIP are 32-bit registers
 * - 64-bit effective addresses: Base/Index/RIP are 64-bit registers or
 *                               explicit memory offsets used.
 *  If ModRM only uses a displacement, we can theoretically use either mode.
 *  (we might have to care about address space wraparound at one point).
 *  Exception: Instructions which have implicit memory operands (like MOVS).
 *  We have to model the addressing mode explicitly. (e.g. MOVS_32)
 *
 * Operand size: Explicitly encoded. E.g. ADD8rm vs. ADD16rm.
 *
 * Prefixes:
 * - Operand-size override: Implicitly managed.
 * - Address-size override: Implicitly managed.
 * - LOCK: FIXME: as possible instruction flag? Otherwise separate opcode
 * - REPNE/REP: Explicitly encoded or for some implicitly managed.
 * - Branch (not) taken: Not supported yet. Implicitly handled.
 * - Segment overrides: Not supported yet. FS/GS might later be modeled as
 *   an additional register in the SIB encoding.
 *
 * General format of opcode identifiers:
 * [NAME][OPERAND SIZE][OPERAND TYPE1][OPERAND TYPE2] ...
 *
 * Operand types:
 *  r: register
 *  m: memory location
 *  i: immediate with operand size (unsigned but not important)
 *  iN: unsigned immediate with different size than operand size
 *  sN: signed immediate with different size than operands size
 *
 *  Especially sN is used when the immediate will be sign-extended to the
 *  full operand size. In that case, some values can not be encoded properly.
 */

#define DEF_OPC(_OPC, ...) _OPC,
enum class Opcode : uint16_t {
    /* Unknown */
    NONE = 0,
#include "Opcodes.inc.h"
    /* number of opcodes */
    MAX,
};
#undef DEF_OPC

typedef enum class Register : uint8_t {
    None = 0,
    /* EFLAGS */
    CF,
    PF,
    AF,
    ZF,
    SF,
    OF,
    /* RAX */
    RAX,
    EAX,
    AX,
    AH,
    AL,
    /* RBX */
    RBX,
    EBX,
    BX,
    BH,
    BL,
    /* RCX */
    RCX,
    ECX,
    CX,
    CH,
    CL,
    /* RDX */
    RDX,
    EDX,
    DX,
    DH,
    DL,
    /* RSI */
    RSI,
    ESI,
    SI,
    SIL,
    /* RDI */
    RDI,
    EDI,
    DI,
    DIL,
    /* RBP */
    RBP,
    EBP,
    BP,
    BPL,
    /* RSP */
    RSP,
    ESP,
    SP,
    SPL,
    /* R8 */
    R8,
    R8D,
    R8W,
    R8B,
    /* R9 */
    R9,
    R9D,
    R9W,
    R9B,
    /* R10 */
    R10,
    R10D,
    R10W,
    R10B,
    /* R11 */
    R11,
    R11D,
    R11W,
    R11B,
    /* R12 */
    R12,
    R12D,
    R12W,
    R12B,
    /* R13 */
    R13,
    R13D,
    R13W,
    R13B,
    /* R14 */
    R14,
    R14D,
    R14W,
    R14B,
    /* R15 */
    R15,
    R15D,
    R15W,
    R15B,
    /* XMM 128-bit registers */
    XMM0,
    XMM1,
    XMM2,
    XMM3,
    XMM4,
    XMM5,
    XMM6,
    XMM7,
    XMM8,
    XMM9,
    XMM10,
    XMM11,
    XMM12,
    XMM13,
    XMM14,
    XMM15,
    MAX,
} Register;

typedef enum class SubRegister : uint8_t {
    /* EFLAGS status flags (for now we don't care about the others) */
    CF = 0,
    PF,
    AF,
    ZF,
    SF,
    OF,
    /*
     * Although the upper 32 bit of a 64bit GPRS are not explicitly
     * addressable (and writing to e.g. EAX will set the upper 32 bit to
     * 0, effectively storing RAX), we have to model RAX separately and
     * make EAX writes also mark RAX as zeroed using a different
     * mechanism.
     *
     * Assume we want to return an int from a function via RAX. The
     * 4 upper bytes are allowed to contain garbage, only the lower
     * 4 bytes (EAX) are well defined. We can't distinguish between
     * RAX and EAX, so we have to assume all 8 bytes are needed. Now,
     * if these 8 bytes were e.g. loaded from the stack (where the
     * bytes are explicitly addressable!), we would now have wrong
     * data dependencies on the 4 upper bytes (that eventually some
     * other instruction temporarily put onto the stack), because we
     * can't model that only 4 bytes are required.
     *
     * For GPRS (e.g. RAX), we'll use the following parts for now
     * SubRegister::A_D1: 63-32
     * SubRegister::A_W1: 31-16
     * SubRegister::A_B1: 15-9
     * SubRegister::A_B0:  8-0
     *
     * The real register masks are than composed of these subregisters.
     * e.g. Register::RAX = A_D1, A_W1, A_B1, A_B0
     * e.g. Register::AX = A_B1, A_B0
     *
     * Modeling these registers completely on byte level might make
     * sense in the future. E.g. If we have an instruction like
     *     DST = SRC AND 0xff00000000000000
     * we would only really read the highest bytes but for now model
     * it as if the complete SRC is read. This will require new
     * arch callbacks to handle these special cases/instructions.
     * For now we don't need it.
     */
    A_B0,
    A_B1,
    A_W1,
    A_D1,
    B_B0,
    B_B1,
    B_W1,
    B_D1,
    C_B0,
    C_B1,
    C_W1,
    C_D1,
    D_B0,
    D_B1,
    D_W1,
    D_D1,
    SI_B0,
    SI_B1,
    SI_W1,
    SI_D1,
    DI_B0,
    DI_B1,
    DI_W1,
    DI_D1,
    BP_B0,
    BP_B1,
    BP_W1,
    BP_D1,
    SP_B0,
    SP_B1,
    SP_W1,
    SP_D1,
    R8_B0,
    R8_B1,
    R8_W1,
    R8_D1,
    R9_B0,
    R9_B1,
    R9_W1,
    R9_D1,
    R10_B0,
    R10_B1,
    R10_W1,
    R10_D1,
    R11_B0,
    R11_B1,
    R11_W1,
    R11_D1,
    R12_B0,
    R12_B1,
    R12_W1,
    R12_D1,
    R13_B0,
    R13_B1,
    R13_W1,
    R13_D1,
    R14_B0,
    R14_B1,
    R14_W1,
    R14_D1,
    R15_B0,
    R15_B1,
    R15_W1,
    R15_D1,
    /*
     * XMM register. Theoretically, we have byte access, however
     * this will require to maintain 16 register for every 128 bit register.
     * Let's do it in blocks of 4 bytes - so we have 4 logical registers
     * per XMM register. For byte or word access, we have to mark
     * instructions to also read the word although they might only
     * write to it. Guess this is not the case to optimize.
     */
    XMM0_D0,
    XMM0_D1,
    XMM0_D2,
    XMM0_D3,
    XMM1_D0,
    XMM1_D1,
    XMM1_D2,
    XMM1_D3,
    XMM2_D0,
    XMM2_D1,
    XMM2_D2,
    XMM2_D3,
    XMM3_D0,
    XMM3_D1,
    XMM3_D2,
    XMM3_D3,
    XMM4_D0,
    XMM4_D1,
    XMM4_D2,
    XMM4_D3,
    XMM5_D0,
    XMM5_D1,
    XMM5_D2,
    XMM5_D3,
    XMM6_D0,
    XMM6_D1,
    XMM6_D2,
    XMM6_D3,
    XMM7_D0,
    XMM7_D1,
    XMM7_D2,
    XMM7_D3,
    XMM8_D0,
    XMM8_D1,
    XMM8_D2,
    XMM8_D3,
    XMM9_D0,
    XMM9_D1,
    XMM9_D2,
    XMM9_D3,
    XMM10_D0,
    XMM10_D1,
    XMM10_D2,
    XMM10_D3,
    XMM11_D0,
    XMM11_D1,
    XMM11_D2,
    XMM11_D3,
    XMM12_D0,
    XMM12_D1,
    XMM12_D2,
    XMM12_D3,
    XMM13_D0,
    XMM13_D1,
    XMM13_D2,
    XMM13_D3,
    XMM14_D0,
    XMM14_D1,
    XMM14_D2,
    XMM14_D3,
    XMM15_D0,
    XMM15_D1,
    XMM15_D2,
    XMM15_D3,
    /* Maximum number of subregisters, defines the bitmap size */
    MAX,
} SubRegister;

}

#endif /* ARCH_DEF_H */
