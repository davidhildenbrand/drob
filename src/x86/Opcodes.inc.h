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

/*
 * General format:
 * - Opcode name
 * - Explicit operands
 * - Implicit operands
 * - Predicate
 * - Opcode Type
 * - Operand refinement callback
 * - Code generation callback
 * - Emulation callback
 * - Specialization callback
 * - Flags (e.g. emulation capabilities)
 */

/* ADD */
DEF_OPC(ADD8mr, m8RW_r8R, eflagsW, none, Other, nullptr, add, add8, nullptr, OfEmuImm)
DEF_OPC(ADD8rr, r8RW_r8R, eflagsW, none, Other, nullptr, add, add8, nullptr, OfEmuImm)
DEF_OPC(ADD8rm, r8RW_m8R, eflagsW, none, Other, nullptr, add, add8, nullptr, OfEmuImm)
DEF_OPC(ADD8mi, m8RW_i8, eflagsW, none, Other, nullptr, add, add8, nullptr, OfEmuImm)
DEF_OPC(ADD8ri, r8RW_i8, eflagsW, none, Other, nullptr, add, add8, nullptr, OfEmuImm)
DEF_OPC(ADD16mr, m16RW_r16R, eflagsW, none, Other, nullptr, add, add16, nullptr, OfEmuImm)
DEF_OPC(ADD16rr, r16RW_r16R, eflagsW, none, Other, nullptr, add, add16, nullptr, OfEmuImm)
DEF_OPC(ADD16rm, r16RW_m16R, eflagsW, none, Other, nullptr, add, add16, nullptr, OfEmuImm)
DEF_OPC(ADD16mi, m16RW_i16, eflagsW, none, Other, nullptr, add, add16, nullptr, OfEmuImm)
DEF_OPC(ADD16ri, r16RW_i16, eflagsW, none, Other, nullptr, add, add16, nullptr, OfEmuImm)
DEF_OPC(ADD32mr, m32RW_r32R, eflagsW, none, Other, nullptr, add, add32, nullptr, OfEmuImm)
DEF_OPC(ADD32rr, r32RW_r32R, eflagsW, none, Other, nullptr, add, add32, nullptr, OfEmuImm)
DEF_OPC(ADD32rm, r32RW_m32R, eflagsW, none, Other, nullptr, add, add32, nullptr, OfEmuImm)
DEF_OPC(ADD32mi, m32RW_i32, eflagsW, none, Other, nullptr, add, add32, nullptr, OfEmuImm)
DEF_OPC(ADD32ri, r32RW_i32, eflagsW, none, Other, nullptr, add, add32, nullptr, OfEmuImm)
DEF_OPC(ADD64mr, m64RW_r64R, eflagsW, none, Other, nullptr, add, add64, add64, OfEmuPtr)
DEF_OPC(ADD64rr, r64RW_r64R, eflagsW, none, Other, nullptr, add, add64, add64, OfEmuPtr)
DEF_OPC(ADD64rm, r64RW_m64R, eflagsW, none, Other, nullptr, add, add64, add64, OfEmuPtr)
DEF_OPC(ADD64mi, m64RW_s32, eflagsW, none, Other, nullptr, add, add64, add64, OfEmuPtr)
DEF_OPC(ADD64ri, r64RW_s32, eflagsW, none, Other, nullptr, add, add64, add64, OfEmuPtr)

/* ADDPD */
DEF_OPC(ADDPDrm, x128RW_m128R, none, none, Other, nullptr, addpd, addpd, addpd, OfEmuImm)
DEF_OPC(ADDPDrr, x128RW_x128R, none, none, Other, nullptr, addpd, addpd, addpd, OfEmuImm)

/* ADDSD */
DEF_OPC(ADDSDrm, x64RW_m64R, none, none, Other, nullptr, addsd, addsd, addsd, OfEmuImm)
DEF_OPC(ADDSDrr, x64RW_x64R, none, none, Other, nullptr, addsd, addsd, addsd, OfEmuImm)

/* CALL */
DEF_OPC(CALLa, mA, push64, none, Call, nullptr, call, call, nullptr, OfEmuFull)
DEF_OPC(CALLm, m64R, push64, none, Call, nullptr, call, call, nullptr, OfEmuFull)
DEF_OPC(CALLr, r64R, push64, none, Call, nullptr, call, call, nullptr, OfEmuFull)
/* Far calls (e.g. JMP m16:64) is not supported yet. */

/* CMP */
DEF_OPC(CMP8mr, m8R_r8R, eflagsW, none, Other, nullptr, cmp, cmp8, cmp8, OfEmuImm)
DEF_OPC(CMP8mi, m8R_i8, eflagsW, none, Other, nullptr, cmp, cmp8, nullptr, OfEmuImm)
DEF_OPC(CMP8rm, r8R_m8R, eflagsW, none, Other, nullptr, cmp, cmp8, cmp8, OfEmuImm)
DEF_OPC(CMP8rr, r8R_r8R, eflagsW, none, Other, nullptr, cmp, cmp8, cmp8, OfEmuImm)
DEF_OPC(CMP8ri, r8R_i8, eflagsW, none, Other, nullptr, cmp, cmp16, nullptr, OfEmuImm)
DEF_OPC(CMP16mr, m16R_r16R, eflagsW, none, Other, nullptr, cmp, cmp16, cmp16, OfEmuImm)
DEF_OPC(CMP16mi, m16R_i16, eflagsW, none, Other, nullptr, cmp, cmp16, nullptr, OfEmuImm)
DEF_OPC(CMP16rm, r16R_m16R, eflagsW, none, Other, nullptr, cmp, cmp16, cmp16, OfEmuImm)
DEF_OPC(CMP16rr, r16R_r16R, eflagsW, none, Other, nullptr, cmp, cmp16, cmp16, OfEmuImm)
DEF_OPC(CMP16ri, r16R_i16, eflagsW, none, Other, nullptr, cmp, cmp16, nullptr, OfEmuImm)
DEF_OPC(CMP32mr, m32R_r32R, eflagsW, none, Other, nullptr, cmp, cmp32, cmp32, OfEmuImm)
DEF_OPC(CMP32mi, m32R_i32, eflagsW, none, Other, nullptr, cmp, cmp32, nullptr, OfEmuImm)
DEF_OPC(CMP32rm, r32R_m32R, eflagsW, none, Other, nullptr, cmp, cmp32, cmp32, OfEmuImm)
DEF_OPC(CMP32rr, r32R_r32R, eflagsW, none, Other, nullptr, cmp, cmp32,cmp32, OfEmuImm)
DEF_OPC(CMP32ri, r32R_i32, eflagsW, none, Other, nullptr, cmp, cmp32, nullptr, OfEmuImm)
DEF_OPC(CMP64mr, m64R_r64R, eflagsW, none, Other, nullptr, cmp, cmp64, cmp64, OfEmuPtr)
DEF_OPC(CMP64mi, m64R_s32, eflagsW, none, Other, nullptr, cmp, cmp64, nullptr, OfEmuPtr)
DEF_OPC(CMP64rm, r64R_m64R, eflagsW, none, Other, nullptr, cmp, cmp64, cmp64, OfEmuPtr)
DEF_OPC(CMP64rr, r64R_r64R, eflagsW, none, Other, nullptr, cmp, cmp64, cmp64, OfEmuPtr)
DEF_OPC(CMP64ri, r64R_s32, eflagsW, none, Other, nullptr, cmp, cmp64, nullptr, OfEmuPtr)

/* Jcc (rel8/32 converted to absolute address) */
DEF_OPC(JNBEa, mA, none, NBE, Branch, nullptr, jcc, nullptr, nullptr, OfNone)
DEF_OPC(JNBa, mA, none, NB, Branch, nullptr, jcc, nullptr, nullptr, OfNone)
DEF_OPC(JBa, mA, none, B, Branch, nullptr, jcc, nullptr, nullptr, OfNone)
DEF_OPC(JBEa, mA, none, BE, Branch, nullptr, jcc, nullptr, nullptr, OfNone)
DEF_OPC(JCXZ32a, mA, none, ECX0, Branch, nullptr, jcc, nullptr, nullptr, OfNone)
DEF_OPC(JCXZ64a, mA, none, RCX0, Branch, nullptr, jcc, nullptr, nullptr, OfNone)
DEF_OPC(JZa, mA, none, Z, Branch, nullptr, jcc, nullptr, nullptr, OfNone)
DEF_OPC(JNLEa, mA, none, NLE, Branch, nullptr, jcc, nullptr, nullptr, OfNone)
DEF_OPC(JNLa, mA, none, NL, Branch, nullptr, jcc, nullptr, nullptr, OfNone)
DEF_OPC(JLa, mA, none, L, Branch, nullptr, jcc, nullptr, nullptr, OfNone)
DEF_OPC(JLEa, mA, none, LE, Branch, nullptr, jcc, nullptr, nullptr, OfNone)
DEF_OPC(JNZa, mA, none, NZ, Branch, nullptr, jcc, nullptr, nullptr, OfNone)
DEF_OPC(JNOa, mA, none, NO, Branch, nullptr, jcc, nullptr, nullptr, OfNone)
DEF_OPC(JNPa, mA, none, NP, Branch, nullptr, jcc, nullptr, nullptr, OfNone)
DEF_OPC(JNSa, mA, none, NS, Branch, nullptr, jcc, nullptr, nullptr, OfNone)
DEF_OPC(JOa, mA, none, O, Branch, nullptr, jcc, nullptr, nullptr, OfNone)
DEF_OPC(JPa, mA, none, P, Branch, nullptr, jcc, nullptr, nullptr, OfNone)
DEF_OPC(JSa, mA, none, S, Branch, nullptr, jcc, nullptr, nullptr, OfNone)

/* JMP */
DEF_OPC(JMPa, mA, none, none, Branch, nullptr, jmp, nullptr, nullptr, OfNone)
DEF_OPC(JMPm, m64R, none, none, Branch, nullptr, jmp, nullptr, nullptr, OfNone)
DEF_OPC(JMPr, r64R, none, none, Branch, nullptr, jmp, nullptr, nullptr, OfNone)
/* Far jumps (e.g. JMP m16:64) is not supported yet. */

/* LEA */
DEF_OPC(LEA64ra, r64W_mA, none, none, Other, nullptr, lea, lea, lea64, OfEmuFull)
DEF_OPC(LEA32ra, r32W_mA, none, none, Other, nullptr, lea, lea, lea32, OfEmuFull)
DEF_OPC(LEA16ra, r16W_mA, none, none, Other, nullptr, lea, lea, lea16, OfEmuFull)

/* MOV */
DEF_OPC(MOV64mr, m64W_r64R, none, none, Other, nullptr, mov, mov, mov64, OfEmuFull)
DEF_OPC(MOV64rr, r64W_r64R, none, none, Other, nullptr, mov, mov, mov64, OfEmuFull)
DEF_OPC(MOV64rm, r64W_m64R, none, none, Other, nullptr, mov, mov, mov64, OfEmuFull)
DEF_OPC(MOV64mi, m64W_s32, none, none, Other, nullptr, mov, mov, nullptr, OfEmuFull)
DEF_OPC(MOV64ri, r64W_i64, none, none, Other, nullptr, mov, mov, nullptr, OfEmuFull)
DEF_OPC(MOV32mr, m32W_r32R, none, none, Other, nullptr, mov, mov, mov32, OfEmuFull)
DEF_OPC(MOV32rr, r32W_r32R, none, none, Other, nullptr, mov, mov, mov32, OfEmuFull)
DEF_OPC(MOV32rm, r32W_m32R, none, none, Other, nullptr, mov, mov, mov32, OfEmuFull)
DEF_OPC(MOV32mi, m32W_i32, none, none, Other, nullptr, mov, mov, nullptr, OfEmuFull)
DEF_OPC(MOV32ri, r32W_i32, none, none, Other, nullptr, mov, mov, nullptr, OfEmuFull)

/* MOVAPD */
DEF_OPC(MOVAPDrm, x128W_m128R, none, none, Other, nullptr, movapd, mov, movapd, OfEmuFull)
DEF_OPC(MOVAPDrr, x128W_x128R, none, none, Other, nullptr, movapd, mov, movapd, OfEmuFull)
DEF_OPC(MOVAPDmr, m128W_x128R, none, none, Other, nullptr, movapd, mov, nullptr, OfEmuFull)

/* MOVSD */
DEF_OPC(MOVSDrm, x64W_m64R, none, none, Other, nullptr, movsd, mov, movsd, OfEmuFull)
DEF_OPC(MOVSDrr, x64W_x64R, none, none, Other, nullptr, movsd, mov, movsd, OfEmuFull)
DEF_OPC(MOVSDmr, m64W_x64R, none, none, Other, nullptr, movsd, mov, movsd, OfEmuFull)

/* MOVUPD */
DEF_OPC(MOVUPDmr, m128W_x128R, none, none, Other, nullptr, movupd, mov, nullptr, OfEmuFull)
DEF_OPC(MOVUPDrr, x128W_m128R, none, none, Other, nullptr, movupd, mov, movupd, OfEmuFull)
DEF_OPC(MOVUPDrm, x128W_m128R, none, none, Other, nullptr, movupd, mov, movupd, OfEmuFull)

/* MOVUPS */
DEF_OPC(MOVUPSmr, m128W_x128R, none, none, Other, nullptr, movups, mov, movapd, OfEmuFull)
DEF_OPC(MOVUPSrr, x128W_x128R, none, none, Other, nullptr, movups, mov, movups, OfEmuFull)
DEF_OPC(MOVUPSrm, x128W_m128R, none, none, Other, nullptr, movups, mov, movups, OfEmuFull)

/* MULPD */
DEF_OPC(MULPDrm, x128RW_m128R, none, none, Other, nullptr, mulpd, mulpd, mulpd, OfEmuImm)
DEF_OPC(MULPDrr, x128RW_x128R, none, none, Other, nullptr, mulpd, mulpd, mulpd, OfEmuImm)

/* MULSD */
DEF_OPC(MULSDrm, x64RW_m64R, none, none, Other, nullptr, mulsd, mulsd, mulsd, OfEmuImm)
DEF_OPC(MULSDrr, x64RW_x64R, none, none, Other, nullptr, mulsd, mulsd, mulsd, OfEmuImm)

/* POP */
DEF_OPC(POP16m, m16W, pop16, none, Other, nullptr, pop, pop, nullptr, OfEmuFull)
DEF_OPC(POP16r, r16W, pop16, none, Other, nullptr, pop, pop, pop, OfEmuFull)
DEF_OPC(POP64m, m64W, pop64, none, Other, nullptr, pop, pop, nullptr, OfEmuFull)
DEF_OPC(POP64r, r64W, pop64, none, Other, nullptr, pop, pop, pop, OfEmuFull)

/* PUSH */
DEF_OPC(PUSH16m, m16R, push16, none, Other, nullptr, push, push, push16, OfEmuFull)
DEF_OPC(PUSH16r, r16R, push16, none, Other, nullptr, push, push, push16, OfEmuFull)
DEF_OPC(PUSH16i, i16, push16, none, Other, nullptr, push, push, nullptr, OfEmuFull)
DEF_OPC(PUSH64m, m64R, push64, none, Other, nullptr, push, push, push64, OfEmuFull)
DEF_OPC(PUSH64r, r64R, push64, none, Other, nullptr, push, push, push64, OfEmuFull)
DEF_OPC(PUSH64i, s32, push64, none, Other, nullptr, push, push, nullptr, OfEmuFull)

/* PXOR
 * - if r1 == r2, r=0 -> No reads!
 * - otherwise: RW destination and R source
 */
DEF_OPC(PXOR128rm, x128RW_m128R, none, none, Other, nullptr, pxor, pxor, pxor, OfEmuFull)
DEF_OPC(PXOR128rr, x128MRW_x128MR, none, none, Other, xor_rr, pxor, pxor, pxor, OfEmuFull)

/* RET */
DEF_OPC(RET, none, ret, none, Ret, nullptr, ret, ret, nullptr, OfEmuFull)

/* SHL
 * - if count is 0, the flags are not written.
 * - if count is !0, the flags are written
 * - if unknown, flags have to be forwarded (may write)
 */
DEF_OPC(SHL64m, m64RW, sh, none, Other, sh, shl, shl64, shl64, OfEmuFull)
DEF_OPC(SHL64r, r64RW, sh, none, Other, sh, shl, shl64, shl64, OfEmuFull)
DEF_OPC(SHL64mi, m64RW_i8, eflagsMW, none, Other, sh, shl, shl64, shl64, OfEmuFull)
DEF_OPC(SHL64ri, r64RW_i8, eflagsMW, none, Other, sh, shl, shl64, shl64, OfEmuFull)
DEF_OPC(SHR64m, m64RW, sh, none, Other, sh, shr, shr64, shr64, OfEmuFull)
DEF_OPC(SHR64r, r64RW, sh, none, Other, sh, shr, shr64, shr64, OfEmuFull)
DEF_OPC(SHR64mi, m64RW_i8, eflagsMW, none, Other, sh, shr, shr64, shr64, OfEmuFull)
DEF_OPC(SHR64ri, r64RW_i8, eflagsMW, none, Other, sh, shr, shr64, shr64, OfEmuFull)

/* SUB */
DEF_OPC(SUB8mr, m8RW_r8R, eflagsW, none, Other, nullptr, sub, sub8, nullptr, OfEmuImm)
DEF_OPC(SUB8rr, r8RW_r8R, eflagsW, none, Other, nullptr, sub, sub8, nullptr, OfEmuImm)
DEF_OPC(SUB8rm, r8RW_m8R, eflagsW, none, Other, nullptr, sub, sub8, nullptr, OfEmuImm)
DEF_OPC(SUB8mi, m8RW_i8, eflagsW, none, Other, nullptr, sub, sub8, nullptr, OfEmuImm)
DEF_OPC(SUB8ri, r8RW_i8, eflagsW, none, Other, nullptr, sub, sub8, nullptr, OfEmuImm)
DEF_OPC(SUB16mr, m16RW_r16R, eflagsW, none, Other, nullptr, sub, sub16, nullptr, OfEmuImm)
DEF_OPC(SUB16rr, r16RW_r16R, eflagsW, none, Other, nullptr, sub, sub16, nullptr, OfEmuImm)
DEF_OPC(SUB16rm, r16RW_m16R, eflagsW, none, Other, nullptr, sub, sub16, nullptr, OfEmuImm)
DEF_OPC(SUB16mi, m16RW_i16, eflagsW, none, Other, nullptr, sub, sub16, nullptr, OfEmuImm)
DEF_OPC(SUB16ri, r16RW_i16, eflagsW, none, Other, nullptr, sub, sub16, nullptr, OfEmuImm)
DEF_OPC(SUB32mr, m32RW_r32R, eflagsW, none, Other, nullptr, sub, sub32, nullptr, OfEmuImm)
DEF_OPC(SUB32rr, r32RW_r32R, eflagsW, none, Other, nullptr, sub, sub32, nullptr, OfEmuImm)
DEF_OPC(SUB32rm, r32RW_m32R, eflagsW, none, Other, nullptr, sub, sub32, nullptr, OfEmuImm)
DEF_OPC(SUB32mi, m32RW_i32, eflagsW, none, Other, nullptr, sub, sub32, nullptr, OfEmuImm)
DEF_OPC(SUB32ri, r32RW_i32, eflagsW, none, Other, nullptr, sub, sub32, nullptr, OfEmuImm)
DEF_OPC(SUB64mr, m64RW_r64R, eflagsW, none, Other, nullptr, sub, sub64, nullptr, OfEmuPtr)
DEF_OPC(SUB64rr, r64RW_r64R, eflagsW, none, Other, nullptr, sub, sub64, nullptr, OfEmuPtr)
DEF_OPC(SUB64rm, r64RW_m64R, eflagsW, none, Other, nullptr, sub, sub64, nullptr, OfEmuPtr)
DEF_OPC(SUB64mi, m64RW_s32, eflagsW, none, Other, nullptr, sub, sub64, nullptr, OfEmuPtr)
DEF_OPC(SUB64ri, r64RW_s32, eflagsW, none, Other, nullptr, sub, sub64, nullptr, OfEmuPtr)

/* TEST - full emulation because we can always set some flags */
DEF_OPC(TEST8mr, m8R_r8R, eflagsW, none, Other, nullptr, test, test8, test8, OfEmuFull)
DEF_OPC(TEST8mi, m8R_i8, eflagsW, none, Other, nullptr, test, test8, nullptr, OfEmuFull)
DEF_OPC(TEST8rr, r8R_r8R, eflagsW, none, Other, nullptr, test, test8, test8, OfEmuFull)
DEF_OPC(TEST8ri, r8R_i8, eflagsW, none, Other, nullptr, test, test8, nullptr, OfEmuFull)
DEF_OPC(TEST16mr, m16R_r16R, eflagsW, none, Other, nullptr, test, test16, test16, OfEmuFull)
DEF_OPC(TEST16mi, m16R_i16, eflagsW, none, Other, nullptr, test, test16, nullptr, OfEmuFull)
DEF_OPC(TEST16rr, r16R_r16R, eflagsW, none, Other, nullptr, test, test16, test16, OfEmuFull)
DEF_OPC(TEST16ri, r16R_i16, eflagsW, none, Other, nullptr, test, test16, nullptr, OfEmuFull)
DEF_OPC(TEST32mr, m32R_r32R, eflagsW, none, Other, nullptr, test, test32, test32, OfEmuFull)
DEF_OPC(TEST32mi, m32R_i32, eflagsW, none, Other, nullptr, test, test32, nullptr, OfEmuFull)
DEF_OPC(TEST32rr, r32R_r32R, eflagsW, none, Other, nullptr, test, test32, test32, OfEmuFull)
DEF_OPC(TEST32ri, r32R_i32, eflagsW, none, Other, nullptr, test, test32, nullptr, OfEmuFull)
DEF_OPC(TEST64mr, m64R_r64R, eflagsW, none, Other, nullptr, test, test64, test64, OfEmuFull)
DEF_OPC(TEST64mi, m64R_s32, eflagsW, none, Other, nullptr, test, test64, nullptr, OfEmuFull)
DEF_OPC(TEST64rr, r64R_r64R, eflagsW, none, Other, nullptr, test, test64, test64, OfEmuFull)
DEF_OPC(TEST64ri, r64R_s32, eflagsW, none, Other, nullptr, test, test64, nullptr, OfEmuFull)

/* XOR
 * - if r1 == r2, r=0 -> No reads!
 * - otherwise: RW destination and R source
 */
DEF_OPC(XOR64mr, m64RW_r64R, eflagsW, none, Other, nullptr, xor, xor64, xor64, OfEmuImm)
DEF_OPC(XOR64rr, r64MRW_r64MR, eflagsW, none, Other, xor_rr, xor, xor64, xor64, OfEmuImm)
DEF_OPC(XOR64rm, r64RW_m64R, eflagsW, none, Other, nullptr, xor, xor64, xor64, OfEmuImm)
DEF_OPC(XOR64mi, m64RW_s32, eflagsW, none, Other, nullptr, xor, xor64, xor64, OfEmuImm)
DEF_OPC(XOR64ri, r64RW_s32, eflagsW, none, Other, nullptr, xor, xor64, xor64, OfEmuImm)
DEF_OPC(XOR32mr, m32RW_r32R, eflagsW, none, Other, nullptr, xor, xor32, xor32, OfEmuImm)
DEF_OPC(XOR32rr, r32MRW_r32MR, eflagsW, none, Other, xor_rr, xor, xor32, xor32, OfEmuImm)
DEF_OPC(XOR32rm, r32RW_m32R, eflagsW, none, Other, nullptr, xor, xor32, xor32, OfEmuImm)
DEF_OPC(XOR32mi, m32RW_i32, eflagsW, none, Other, nullptr, xor, xor32, xor32, OfEmuImm)
DEF_OPC(XOR32ri, r32RW_i32, eflagsW, none, Other, nullptr, xor, xor32, xor32, OfEmuImm)
