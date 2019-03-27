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
#include "Specialize.hpp"
#include "../InstructionInfo.hpp"
#include "../RewriterCfg.hpp"
#include "../BinaryPool.hpp"
#include "../Instruction.hpp"
#include "x86.hpp"
#include "gen/gen-defs.h"

namespace drob {

static const SubRegisterMask eflags = {
    .m = { SUBREGISTER_MASK_EFLAGS }
};

static inline bool registersWillBeRead(const LivenessData &livenessData,
                                       const SubRegisterMask &registers)
{
    return !!(livenessData.live_out & registers);
}

/*
 * Whenever we encode a UsrPtr value as immediate, we don't want to lose
 * the pointer information. Otherwise our StackAnalysis after modifying an
 * instruction might go nuts. Maintain how the value was calculated.
 *
 * This assures that when we do delta stack analysis to regenerate the dynamic
 * instruction info, that
 * - Values that used to be read via the pointer ("const usr pointer") can still
 *   be read and are not suddenly "Unknown"
 * - ProgramState will not suddenly have a mixture of immediates and pointers
 *   at the place where the usr ptr was.
 */
static bool getImm(const Data &data, Immediate64 &imm, const RewriterCfg &cfg)
{
    if (likely(data.isImm())) {
        drob_assert(!data.isImm128());
        imm.val = data.getImm64();
        imm.usrPtrNr = -1;
        return true;
    } else if (data.isUsrPtr()) {
        const UsrPtrCfg ptrCfg = cfg.getUsrPtrCfg(data.getNr());

        if (ptrCfg.isKnown) {
            imm.val = ((int64_t)ptrCfg.val + data.getPtrOffset());
            imm.usrPtrNr = data.getNr();
            imm.usrPtrOffset = data.getPtrOffset();
            return true;
        }
    }
    return false;
}

/*
 * TODO: We should eventually have some drob cfg whether to move constants
 * from register to memory (e.g. ADDPDrr -> ADDPDrm).
 *
 * Sometimes it is beneficial (e.g. think about we had a MOVUPDrm just before
 * the instruction), sometimes not (register can't be optimized out).
 *
 * At least for MOVs it does not really matter, MOVrr is just a register
 * renaming, so for these it most probably makes no sense.
 *
 * But as we use a constant pool, loading from memory might not be that bad?
 */

DEF_SPECIALIZE_FN(add64)
{
    const bool eflagsRead = registersWillBeRead(livenessData, eflags);
    Immediate64 imm;

    /* TODO:
     * we could convert to LEA if eflags not read -> might harm performance?
     * we could convert to INC if eflags are not read -> also bad for performance?
     */

    /* If eflags are not of interest, we can do some nice optimizations */
    if (!eflagsRead) {
        /* RHS is 0 -> the instruction has no effect */
        if (getImm(dynInfo.operands[1].input, imm, cfg) && !imm.val) {
            return SpecRet::Delete;
        }

        /* Output known ? */
        if (getImm(dynInfo.operands[0].output, imm, cfg)) {
            if (opcode == Opcode::ADD64mr || opcode == Opcode::ADD64mi) {
                if (is_simm32(imm.val)) {
                    opcode = Opcode::MOV64mi;
                    explOperands.op[1].imm = imm;
                    return SpecRet::Change;
                }
            } else {
                /* result is 0 - use XORrr */
                if (imm.val == 0) {
                    /* TODO: use XOR32rr -> have to translate register */
                    opcode = Opcode::XOR64rr;
                    explOperands.op[1].reg = explOperands.op[0].reg;
                    return SpecRet::Change;
                }
                opcode = Opcode::MOV64ri;
                explOperands.op[1].imm = imm;
                return SpecRet::Change;
            }
        }

        /* LHS is 0 -> simple MOV */
        if (getImm(dynInfo.operands[0].input, imm, cfg) && !imm.val) {
            if (opcode == Opcode::ADD64rr) {
                opcode = Opcode::MOV64rr;
                return SpecRet::Change;
            } else if (opcode == Opcode::ADD64mr) {
                opcode = Opcode::MOV64mr;
                return SpecRet::Change;
            } else if (opcode == Opcode::ADD64rm) {
                opcode = Opcode::MOV64rm;
                return SpecRet::Change;
            }
        }
    }

    if (opcode == Opcode::ADD64mi || opcode == Opcode::ADD64ri) {
        return SpecRet::NoChange;
    }

    /* RHS known and eflags relevant */
    if (getImm(dynInfo.operands[1].input, imm, cfg)) {
        if (opcode == Opcode::ADD64mr) {
            if (is_simm32(imm.val)) {
                opcode = Opcode::ADD64mi;
                explOperands.op[1].imm = imm;
                return SpecRet::Change;
            }
        } else if (is_simm32(imm.val)) {
            opcode = Opcode::ADD64ri;
            explOperands.op[1].imm = imm;
            return SpecRet::Change;
        } else if (imm.usrPtrNr < 0) {
            /* don't move UsrPtr to the constant pool */
            opcode = Opcode::ADD64rm;
            explOperands.op[1].mem.type = MemPtrType::Direct;
            explOperands.op[1].mem.addr.val = (uint64_t)binaryPool.allocConstant(imm.val);
            explOperands.op[1].mem.addr.usrPtrNr = -1;;
            return SpecRet::Change;
        }
    }

    return SpecRet::NoChange;
}

DEF_SPECIALIZE_FN(addpd)
{
    __uint128_t imm;

    /*
     * TODO: adding zero can only sometimes be optimized out (due to fp
     * arithmetic and signs).
     */
    if (dynInfo.operands[0].output.isImm()) {
        imm = dynInfo.operands[0].output.getImm128();
        if (!imm) {
            /* result is 0 -> use PXOR */
            opcode = Opcode::PXOR128rr;
            explOperands.op[1].reg = explOperands.op[0].reg;
            return SpecRet::Change;
        }

        /* use MOVAPD - constant pool gives us aligned pointers */
        opcode = Opcode::MOVAPDrm;
        explOperands.op[1].mem.type = MemPtrType::Direct;
        explOperands.op[1].mem.addr.val = (uint64_t)binaryPool.allocConstant(imm);
        explOperands.op[1].mem.addr.usrPtrNr = -1;
        return SpecRet::Change;
    }

    if (dynInfo.operands[1].input.isImm()) {
        imm = dynInfo.operands[1].input.getImm128();
        opcode = Opcode::ADDPDrm;
        explOperands.op[1].mem.type = MemPtrType::Direct;
        explOperands.op[1].mem.addr.val = (uint64_t)binaryPool.allocConstant(imm);
        explOperands.op[1].mem.addr.usrPtrNr = -1;
        return SpecRet::Change;
    }

    return SpecRet::NoChange;
}

DEF_SPECIALIZE_FN(addsd)
{
    uint64_t imm;

    /*
     * TODO: adding zero can only sometimes be optimized out (due to fp
     * arithmetic and signs).
     */
    if (dynInfo.operands[0].output.isImm()) {
        imm = dynInfo.operands[0].output.getImm64();
        /* TODO: If !imm and high part of register is not alive, use PXOR */

        /* use MOVSD */
        opcode = Opcode::MOVSDrm;
        explOperands.op[1].mem.type = MemPtrType::Direct;
        explOperands.op[1].mem.addr.val = (uint64_t)binaryPool.allocConstant(imm);
        explOperands.op[1].mem.addr.usrPtrNr = -1;
        return SpecRet::Change;
    }

    if (dynInfo.operands[1].input.isImm()) {
        imm = dynInfo.operands[1].input.getImm64();
        opcode = Opcode::ADDSDrm;
        explOperands.op[1].mem.type = MemPtrType::Direct;
        explOperands.op[1].mem.addr.val = (uint64_t)binaryPool.allocConstant(imm);
        explOperands.op[1].mem.addr.usrPtrNr = -1;
        return SpecRet::Change;
    }

    return SpecRet::NoChange;
}

DEF_SPECIALIZE_FN(cmp8)
{
    Immediate64 imm;

    if (getImm(dynInfo.operands[1].input, imm, cfg)) {
        if (opcode == Opcode::CMP8mr) {
            opcode = Opcode::CMP8mi;
            explOperands.op[1].imm = imm;
            return SpecRet::Change;
        } else {
            opcode = Opcode::CMP8ri;
            explOperands.op[1].imm = imm;
            return SpecRet::Change;
        }
    }
    return SpecRet::NoChange;
}

DEF_SPECIALIZE_FN(cmp16)
{
    Immediate64 imm;

    if (getImm(dynInfo.operands[1].input, imm, cfg)) {
        if (opcode == Opcode::CMP16mr) {
            opcode = Opcode::CMP16mi;
            explOperands.op[1].imm = imm;
            return SpecRet::Change;
        } else {
            opcode = Opcode::CMP16ri;
            explOperands.op[1].imm = imm;
            return SpecRet::Change;
        }
    }
    return SpecRet::NoChange;
}

DEF_SPECIALIZE_FN(cmp32)
{
    Immediate64 imm;

    if (getImm(dynInfo.operands[1].input, imm, cfg)) {
        if (opcode == Opcode::CMP32mr) {
            opcode = Opcode::CMP32mi;
            explOperands.op[1].imm = imm;
            return SpecRet::Change;
        } else {
            opcode = Opcode::CMP32ri;
            explOperands.op[1].imm = imm;
            return SpecRet::Change;
        }
    }
    return SpecRet::NoChange;
}

DEF_SPECIALIZE_FN(cmp64)
{
    Immediate64 imm;

    if (getImm(dynInfo.operands[1].input, imm, cfg)) {
        if (is_simm32(imm.val)) {
            if (opcode == Opcode::CMP64mr) {
                opcode = Opcode::CMP64mi;
                explOperands.op[1].imm = imm;
                return SpecRet::Change;
            } else {
                opcode = Opcode::CMP64ri;
                explOperands.op[1].imm = imm;
                return SpecRet::Change;
            }
        } else if (imm.usrPtrNr < 0 &&
                   (opcode == Opcode::CMP64rr || opcode == Opcode::CMP64rm)) {
            /* don't move UsrPtr to the constant pool */
            opcode = Opcode::CMP64rm;
            explOperands.op[1].mem.type = MemPtrType::Direct;
            explOperands.op[1].mem.addr.val = (uint64_t)binaryPool.allocConstant(imm.val);
            explOperands.op[1].mem.addr.usrPtrNr = -1;
            return SpecRet::Change;
        }
    }
    return SpecRet::NoChange;
}

DEF_SPECIALIZE_FN(lea64)
{
    Immediate64 imm;

    if (getImm(dynInfo.operands[0].output, imm, cfg)) {
        if (imm.usrPtrNr < 0 && !imm.val &&
            !registersWillBeRead(livenessData, eflags)) {
            /* TODO use XOR32rr -> convert register */
            opcode = Opcode::XOR64rr;
            explOperands.op[1].reg = explOperands.op[0].reg;
            return SpecRet::Change;
        } else {
            /* TODO use MOV32ri if is_imm(imm) -> convert register */
            /*
             * This properly makes sure that some addresses from new code
             * (e.g. LEA %RAX, [%RIP + X]) suddenly become reachable :)
             */
            opcode = Opcode::MOV64ri;
            explOperands.op[1].imm = imm;
            return SpecRet::Change;
        }
    }
    return SpecRet::NoChange;
}

DEF_SPECIALIZE_FN(lea32)
{
    Immediate64 imm;

    if (getImm(dynInfo.operands[0].output, imm, cfg)) {
        opcode = Opcode::MOV32ri;
        explOperands.op[1].imm = imm;
        return SpecRet::Change;
    }
    return SpecRet::NoChange;
}

DEF_SPECIALIZE_FN(lea16)
{
    /* TODO: Implement MOV16ri */
    return SpecRet::NoChange;
}

DEF_SPECIALIZE_FN(mov32)
{
    Immediate64 imm;

    if (unlikely(opcode == Opcode::MOV32rr &&
                 explOperands.op[0].reg == explOperands.op[1].reg)) {
        return SpecRet::Delete;
    }

    if (getImm(dynInfo.operands[1].input, imm, cfg)) {
        explOperands.op[1].imm = imm;
        if (opcode == Opcode::MOV32mr) {
            opcode = Opcode::MOV32mi;
            return SpecRet::Change;
        } else {
            opcode = Opcode::MOV32ri;
            return SpecRet::Change;
        }
    }
    return SpecRet::NoChange;
}

DEF_SPECIALIZE_FN(mov64)
{
    Immediate64 imm;

    if (unlikely(opcode == Opcode::MOV64rr &&
                 explOperands.op[0].reg == explOperands.op[1].reg)) {
        return SpecRet::Delete;
    }

    if (getImm(dynInfo.operands[1].input, imm, cfg)) {
        if (opcode == Opcode::MOV64mr) {
            if (is_simm32(imm.val)) {
                opcode = Opcode::MOV64mi;
                explOperands.op[1].imm = imm;
                return SpecRet::Change;
            }
        } else {
            opcode = Opcode::MOV64ri;
            explOperands.op[1].imm = imm;
            return SpecRet::Change;
        }
    }
    return SpecRet::NoChange;
}

/*
 * "In older processors, there was a significant performance penalty for using
 *  the unaligned versions, even when the data was actually aligned.
 *  These penalties disappeared several generations ago, and the penalties for
 *  unaligned accesses have been reduced in each processor generation."
 *  https://software.intel.com/en-us/forums/intel-isa-extensions/topic/780640
 */

DEF_SPECIALIZE_FN(movapd)
{
    __uint128_t imm;

    if (unlikely(opcode == Opcode::MOVAPDrr &&
                 explOperands.op[0].reg == explOperands.op[1].reg)) {
        return SpecRet::Delete;
    }

    if (dynInfo.operands[0].output.isImm()) {
        imm = dynInfo.operands[0].output.getImm128();
        if (!imm) {
            opcode = Opcode::PXOR128rr;
            explOperands.op[1].reg = explOperands.op[0].reg;
            return SpecRet::Change;
        } else if (opcode == Opcode::MOVAPDrm) {
            explOperands.op[1].mem.type = MemPtrType::Direct;
            explOperands.op[1].mem.addr.val = (uint64_t)binaryPool.allocConstant(imm);
            explOperands.op[1].mem.addr.usrPtrNr = -1;
            return SpecRet::Change;
        }
    }
    return SpecRet::NoChange;
}

DEF_SPECIALIZE_FN(movsd)
{
    uint64_t imm;

    if (unlikely(opcode == Opcode::MOVSDrr &&
                 explOperands.op[0].reg == explOperands.op[1].reg)) {
        return SpecRet::Delete;
    }

    if (dynInfo.operands[0].output.isImm()) {
        imm = dynInfo.operands[0].output.getImm64();
        if (opcode == Opcode::MOVSDrm) {
            opcode = Opcode::MOVSDrm;
            explOperands.op[1].mem.type = MemPtrType::Direct;
            explOperands.op[1].mem.addr.val = (uint64_t)binaryPool.allocConstant(imm);
            explOperands.op[1].mem.addr.usrPtrNr = -1;
            return SpecRet::Change;
        } else {
            /* We can directly move an immediate to memory */
            if (is_simm32(imm)) {
                opcode = Opcode::MOV64mi;
                explOperands.op[1].imm.val = imm;
                explOperands.op[1].imm.usrPtrNr = -1;
                return SpecRet::Change;
            }
        }
    }
    return SpecRet::NoChange;
}

DEF_SPECIALIZE_FN(movupd)
{
    __uint128_t imm;

    if (unlikely(opcode == Opcode::MOVUPDrr &&
                 explOperands.op[0].reg == explOperands.op[1].reg)) {
        return SpecRet::Delete;
    }

    if (dynInfo.operands[0].output.isImm()) {
        imm = dynInfo.operands[0].output.getImm128();
        if (!imm) {
            opcode = Opcode::PXOR128rr;
            explOperands.op[1].reg = explOperands.op[0].reg;
            return SpecRet::Change;
        } else if (opcode == Opcode::MOVUPDrm) {
            opcode = Opcode::MOVAPDrm;
            explOperands.op[1].mem.type = MemPtrType::Direct;
            explOperands.op[1].mem.addr.val = (uint64_t)binaryPool.allocConstant(imm);
            explOperands.op[1].mem.addr.usrPtrNr = -1;
            return SpecRet::Change;
        }
    }

    /*
     * TODO: Convert to MOVAPD if possible - although it might not matter for
     * performance, it allows us to easily convert
     *
     * movupd xmm0, xmmword ptr [r9+rax*1]
     * addpd xmm0, xmm4
     *
     * into
     *
     * addpd xmm0, xmmword ptr [r9+rax*1]
     *
     * ... or detect this via memory pointers somehow "last one writing xmm0" :)
     */
    return SpecRet::NoChange;
}

DEF_SPECIALIZE_FN(movups)
{
    __uint128_t imm;

    if (unlikely(opcode == Opcode::MOVUPSrr &&
                 explOperands.op[0].reg == explOperands.op[1].reg)) {
        return SpecRet::Delete;
    }

    if (dynInfo.operands[0].output.isImm() && opcode == Opcode::MOVUPSrm) {
        imm = dynInfo.operands[0].output.getImm128();
        explOperands.op[1].mem.type = MemPtrType::Direct;
        explOperands.op[1].mem.addr.val = (uint64_t)binaryPool.allocConstant(imm);
        explOperands.op[1].mem.addr.usrPtrNr = -1;
        return SpecRet::Change;
    }
    return SpecRet::NoChange;
}

DEF_SPECIALIZE_FN(mulpd)
{
    __uint128_t imm;

    if (dynInfo.operands[0].output.isImm()) {
        imm = dynInfo.operands[0].output.getImm128();
        if (!imm) {
            opcode = Opcode::PXOR128rr;
            explOperands.op[1].reg = explOperands.op[0].reg;
            return SpecRet::Change;
        }
        opcode = Opcode::MOVAPDrm;
        explOperands.op[1].mem.type = MemPtrType::Direct;
        explOperands.op[1].mem.addr.val = (uint64_t)binaryPool.allocConstant(imm);
        explOperands.op[1].mem.addr.usrPtrNr = -1;
        return SpecRet::Change;
    }

    if (dynInfo.operands[1].input.isImm()) {
        imm = dynInfo.operands[1].input.getImm128();
        opcode = Opcode::MULPDrm;
        explOperands.op[1].mem.type = MemPtrType::Direct;
        explOperands.op[1].mem.addr.val = (uint64_t)binaryPool.allocConstant(imm);
        explOperands.op[1].mem.addr.usrPtrNr = -1;
        return SpecRet::Change;
    }
    return SpecRet::NoChange;
}

DEF_SPECIALIZE_FN(mulsd)
{
    uint64_t imm;

    if (dynInfo.operands[0].output.isImm()) {
        imm = dynInfo.operands[0].output.getImm64();
        opcode = Opcode::MOVSDrm;
        explOperands.op[1].mem.type = MemPtrType::Direct;
        explOperands.op[1].mem.addr.val = (uint64_t)binaryPool.allocConstant(imm);
        explOperands.op[1].mem.addr.usrPtrNr = -1;
        return SpecRet::Change;
    }

    if (dynInfo.operands[1].input.isImm()) {
        imm = dynInfo.operands[1].input.getImm64();
        opcode = Opcode::MULSDrm;
        explOperands.op[1].mem.type = MemPtrType::Direct;
        explOperands.op[1].mem.addr.val = (uint64_t)binaryPool.allocConstant(imm);
        explOperands.op[1].mem.addr.usrPtrNr = -1;
        return SpecRet::Change;
    }
    return SpecRet::NoChange;
}

DEF_SPECIALIZE_FN(pop)
{
    /* if nobody reads the register, only calculate the RSP */
    if (!registersWillBeRead(livenessData, getSubRegisterMask(explOperands.op[0].reg))) {
        opcode = Opcode::LEA64ra;
        explOperands.op[0].reg = Register::RSP;
        explOperands.op[1].mem.type = MemPtrType::SIB;
        explOperands.op[1].mem.sib.base = Register::RSP;
        explOperands.op[1].mem.sib.index = Register::None;
        explOperands.op[1].mem.sib.disp.val = opcode == Opcode::POP64r ? 8 : 2;
        explOperands.op[1].mem.sib.disp.usrPtrNr = -1;
    }

    return SpecRet::NoChange;
}

DEF_SPECIALIZE_FN(push16)
{
    Immediate64 imm;

    if (getImm(dynInfo.operands[0].input, imm, cfg)) {
        opcode = Opcode::PUSH16i;
        explOperands.op[0].imm = imm;
        return SpecRet::Change;
    }
    return SpecRet::NoChange;
}

DEF_SPECIALIZE_FN(push64)
{
    Immediate64 imm;

    if (getImm(dynInfo.operands[0].input, imm, cfg) &&
        is_simm32(imm.val)) {
        opcode = Opcode::PUSH64i;
        explOperands.op[0].imm = imm;
        return SpecRet::Change;
    }
    return SpecRet::NoChange;
}

DEF_SPECIALIZE_FN(pxor)
{
    __uint128_t imm;

    if (opcode == Opcode::PXOR128rr &&
        explOperands.op[0].reg == explOperands.op[1].reg) {
        return SpecRet::NoChange;
    }

    if (dynInfo.operands[0].output.isImm()) {
        imm = dynInfo.operands[0].output.getImm128();

        /* use MOVAPD - constant pool gives us aligned pointers */
        opcode = Opcode::MOVAPDrm;
        explOperands.op[1].mem.type = MemPtrType::Direct;
        explOperands.op[1].mem.addr.val = (uint64_t)binaryPool.allocConstant(imm);
        explOperands.op[1].mem.addr.usrPtrNr = -1;
        return SpecRet::Change;
    }

    if (dynInfo.operands[1].input.isImm()) {
        imm = dynInfo.operands[1].input.getImm128();
        opcode = Opcode::PXOR128rm;
        explOperands.op[1].mem.type = MemPtrType::Direct;
        explOperands.op[1].mem.addr.val = (uint64_t)binaryPool.allocConstant(imm);
        explOperands.op[1].mem.addr.usrPtrNr = -1;
        return SpecRet::Change;
    }
    return SpecRet::NoChange;
}

DEF_SPECIALIZE_FN(shl64)
{
    const bool eflagsRead = registersWillBeRead(livenessData, eflags);
    Immediate64 imm;

    /* zero shifts can be dropped - eflags are not set for shift==0 */
    if (getImm(dynInfo.operands[1].input, imm, cfg) && !(imm.val & 0x3f)) {
        return SpecRet::Delete;
    }

    /* result known and eflags not relevant? */
    if (getImm(dynInfo.operands[0].output, imm, cfg) && !eflagsRead) {
        if (opcode == Opcode::SHL64m || opcode == Opcode::SHL64mi) {
            if (is_simm32(imm.val)) {
                opcode = Opcode::MOV64mi;
                explOperands.op[1].imm = imm;
                return SpecRet::Change;
            }
        } else {
            /* result is 0 - use XORrr */
            if (imm.val == 0) {
                /* TODO: use XOR32rr -> have to translate register */
                opcode = Opcode::XOR64rr;
                explOperands.op[1].reg = explOperands.op[0].reg;
                return SpecRet::Change;
            }
            opcode = Opcode::MOV64ri;
            explOperands.op[1].imm = imm;
            return SpecRet::Change;
        }
    }

    if (opcode != Opcode::SHL64m || opcode != Opcode::SHL64r) {
        return SpecRet::NoChange;
    }

    /* RHS (CL register) can be encoded as immediate? */
    if (getImm(dynInfo.operands[1].input, imm, cfg)) {

        if (opcode == Opcode::SHL64m) {
            opcode = Opcode::SHL64mi;
            explOperands.op[1].imm = imm;
            return SpecRet::Change;
        } else {
            opcode = Opcode::SHL64ri;
            explOperands.op[1].imm = imm;
            return SpecRet::Change;
        }
    }
    return SpecRet::NoChange;
}

DEF_SPECIALIZE_FN(shr64)
{
    const bool eflagsRead = registersWillBeRead(livenessData, eflags);
    Immediate64 imm;

    /* zero shifts can be dropped - eflags are not set for shift==0 */
    if (getImm(dynInfo.operands[1].input, imm, cfg) && !(imm.val & 0x3f)) {
        return SpecRet::Delete;
    }

    /* result known and eflags not relevant? */
    if (getImm(dynInfo.operands[0].output, imm, cfg) && !eflagsRead) {
        if (opcode == Opcode::SHR64m || opcode == Opcode::SHR64mi) {
            if (is_simm32(imm.val)) {
                opcode = Opcode::MOV64mi;
                explOperands.op[1].imm = imm;
                return SpecRet::Change;
            }
        } else {
            /* result is 0 - use XORrr */
            if (imm.val == 0) {
                /* TODO: use XOR32rr -> have to translate register */
                opcode = Opcode::XOR64rr;
                explOperands.op[1].reg = explOperands.op[0].reg;
                return SpecRet::Change;
            }
            opcode = Opcode::MOV64ri;
            explOperands.op[1].imm = imm;
            return SpecRet::Change;
        }
    }

    if (opcode != Opcode::SHR64m || opcode != Opcode::SHR64r) {
        return SpecRet::NoChange;
    }

    /* RHS (CL register) can be encoded as immediate? */
    if (getImm(dynInfo.operands[1].input, imm, cfg)) {

        if (opcode == Opcode::SHR64m) {
            opcode = Opcode::SHR64mi;
            explOperands.op[1].imm = imm;
            return SpecRet::Change;
        } else {
            opcode = Opcode::SHR64ri;
            explOperands.op[1].imm = imm;
            return SpecRet::Change;
        }
    }
    return SpecRet::NoChange;
}

DEF_SPECIALIZE_FN(test8)
{
    Immediate64 imm;

    if (opcode == Opcode::TEST8rr &&
        explOperands.op[0].reg == explOperands.op[1].reg) {
        return SpecRet::NoChange;
    }

    if (getImm(dynInfo.operands[1].input, imm, cfg)) {
        if (opcode == Opcode::TEST8mr) {
            opcode = Opcode::TEST8mi;
            explOperands.op[1].imm = imm;
            return SpecRet::Change;
        } else {
            opcode = Opcode::TEST8ri;
            explOperands.op[1].imm = imm;
            return SpecRet::Change;
        }
    }

    /* We can switch operand positions to encode the LHS as an immediate */
    if (getImm(dynInfo.operands[0].input, imm, cfg)) {
        opcode = Opcode::TEST8ri;
        explOperands.op[0].reg = explOperands.op[1].reg;
        explOperands.op[1].imm = imm;
        return SpecRet::Change;
    }
    return SpecRet::NoChange;
}

DEF_SPECIALIZE_FN(test16)
{
    Immediate64 imm;

    if (opcode == Opcode::TEST16rr &&
        explOperands.op[0].reg == explOperands.op[1].reg) {
        return SpecRet::NoChange;
    }

    if (getImm(dynInfo.operands[1].input, imm, cfg)) {
        if (opcode == Opcode::TEST16mr) {
            opcode = Opcode::TEST16mi;
            explOperands.op[1].imm = imm;
            return SpecRet::Change;
        } else {
            opcode = Opcode::TEST16ri;
            explOperands.op[1].imm = imm;
            return SpecRet::Change;
        }
    }

    /* We can switch operand positions to encode the LHS as an immediate */
    if (getImm(dynInfo.operands[0].input, imm, cfg)) {
        opcode = Opcode::TEST16ri;
        explOperands.op[0].reg = explOperands.op[1].reg;
        explOperands.op[1].imm = imm;
        return SpecRet::Change;
    }
    return SpecRet::NoChange;
}

DEF_SPECIALIZE_FN(test32)
{
    Immediate64 imm;

    if (opcode == Opcode::TEST32rr &&
        explOperands.op[0].reg == explOperands.op[1].reg) {
        return SpecRet::NoChange;
    }

    if (getImm(dynInfo.operands[1].input, imm, cfg)) {
        if (opcode == Opcode::TEST32mr) {
            opcode = Opcode::TEST32mi;
            explOperands.op[1].imm = imm;
            return SpecRet::Change;
        } else {
            opcode = Opcode::TEST32ri;
            explOperands.op[1].imm = imm;
            return SpecRet::Change;
        }
    }

    /* We can switch operand positions to encode the LHS as an immediate */
    if (getImm(dynInfo.operands[0].input, imm, cfg)) {
        opcode = Opcode::TEST32ri;
        explOperands.op[0].reg = explOperands.op[1].reg;
        explOperands.op[1].imm = imm;
        return SpecRet::Change;
    }
    return SpecRet::NoChange;
}

DEF_SPECIALIZE_FN(test64)
{
    Immediate64 imm;

    if (opcode == Opcode::TEST64rr &&
        explOperands.op[0].reg == explOperands.op[1].reg) {
        return SpecRet::NoChange;
    }

    if (getImm(dynInfo.operands[1].input, imm, cfg)) {
        if (is_simm32(imm.val)) {
            if (opcode == Opcode::TEST64mr) {
                opcode = Opcode::TEST64mi;
                explOperands.op[1].imm = imm;
                return SpecRet::Change;
            } else {
                opcode = Opcode::TEST64ri;
                explOperands.op[1].imm = imm;
                return SpecRet::Change;
            }
        }
    }

    /* We can switch operand positions to encode the LHS as an immediate */
    if (getImm(dynInfo.operands[0].input, imm, cfg)) {
        if (is_simm32(imm.val)) {
            opcode = Opcode::TEST64ri;
            explOperands.op[0].reg = explOperands.op[1].reg;
            explOperands.op[1].imm = imm;
            return SpecRet::Change;
        }
    }
    return SpecRet::NoChange;
}

DEF_SPECIALIZE_FN(xor32)
{
    const bool eflagsRead = registersWillBeRead(livenessData, eflags);
    Immediate64 imm;

    if (opcode == Opcode::XOR32rr &&
        explOperands.op[0].reg == explOperands.op[1].reg) {
        return SpecRet::NoChange;
    }

    if (getImm(dynInfo.operands[1].input, imm, cfg)) {
        /* XOR with 0 and eflags not read -> we can drop it */
        if (!imm.val && !eflagsRead) {
            return SpecRet::Delete;
        }

        if (opcode == Opcode::XOR32mr) {
            opcode = Opcode::XOR32mi;
            explOperands.op[1].imm = imm;
            return SpecRet::Change;
        } else if (opcode == Opcode::XOR32rr || opcode == Opcode::XOR32rm) {
            opcode = Opcode::XOR32ri;
            explOperands.op[1].imm = imm;
            return SpecRet::Change;
        }
    }
    return SpecRet::NoChange;
}

DEF_SPECIALIZE_FN(xor64)
{
    const bool eflagsRead = registersWillBeRead(livenessData, eflags);
    Immediate64 imm;

    if (opcode == Opcode::XOR64rr &&
        explOperands.op[0].reg == explOperands.op[1].reg) {
        return SpecRet::NoChange;
    }

    if (getImm(dynInfo.operands[1].input, imm, cfg)) {

        /* XOR with 0 and eflags not read -> we can drop it */
        if (!imm.val && !eflagsRead) {
            return SpecRet::Delete;
        }

        if (is_simm32(imm.val)) {
            if (opcode == Opcode::XOR64mr) {
                opcode = Opcode::XOR64mi;
                explOperands.op[1].imm = imm;
                return SpecRet::Change;
            } else if (opcode == Opcode::XOR64rr || opcode == Opcode::XOR64rm) {
                opcode = Opcode::XOR64ri;
                explOperands.op[1].imm = imm;
                return SpecRet::Change;
            }
        } else if (imm.usrPtrNr < 0 &&
                   (opcode == Opcode::XOR64rr || opcode == Opcode::XOR64rm)) {
            /* don't move UsrPtr to the constant pool */
            opcode = Opcode::XOR64rm;
            explOperands.op[1].mem.type = MemPtrType::Direct;
            explOperands.op[1].mem.addr.val = (uint64_t)binaryPool.allocConstant(imm.val);
            explOperands.op[1].mem.addr.usrPtrNr = -1;
            return SpecRet::Change;
        }
    }
    return SpecRet::NoChange;
}

} /* namespace drob */
