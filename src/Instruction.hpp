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
#ifndef INSTRUCTION_HPP
#define INSTRUCTION_HPP

#include "Utils.hpp"

#include <cstring>
#include <vector>
#include <memory>
#include "arch.hpp"
#include "OpcodeInfo.hpp"
#include "MemProtCache.hpp"
#include "ProgramState.hpp"
#include "InstructionInfo.hpp"

namespace drob {

struct CallEdge;
struct BranchEdge;
struct ReturnEdge;
class BinaryPool;

/* TODO: this belongs somewehre else */
typedef struct LivenessData {
    /*
     * Registers that are alive after the instruction (a.k.a. will be read
     * by following instructions)
     */
    SubRegisterMask live_out;
    /*
     * Registers that are alive before the instruction (a.k.a. will be read
     * by this or following instructions)
     */
    SubRegisterMask live_in;
} LivenessData;

class Instruction {
public:
    Instruction(const uint8_t *itext, uint8_t ilen, Opcode opcode,
                const ExplicitStaticOperands &operands, const OpcodeInfo *opcodeInfo,
                bool reencode) :
            itext(itext), ilen(ilen), reencode(reencode), opcode(opcode),
            rawOperands(operands), opcodeInfo(opcodeInfo)
    {
    }
    Instruction(Opcode opcode, const ExplicitStaticOperands &operands) :
            itext(nullptr), ilen(0), reencode(true), opcode(opcode),
            rawOperands(operands), opcodeInfo(arch_get_opcode_info(opcode))
    {
    }
    ~Instruction(void) = default;
    Instruction(const Instruction &rhs)
    {
        itext = rhs.itext;
        ilen = rhs.ilen;
        reencode = rhs.reencode;
        opcode = rhs.opcode;
        rawOperands = rhs.rawOperands;
        opcodeInfo = rhs.opcodeInfo;

        /*
         * Edges and analysis information is not copied.
         */
    }

    /*
     * Return the opcode.
     */
    Opcode getOpcode() const
    {
        return opcode;
    }

    /*
     * Set the opcode.
     */
    void setOpcode(Opcode opcode)
    {
        modified();
        this->opcode = opcode;
        opcodeInfo = arch_get_opcode_info(opcode);
    }

    /*
     * Get the number of operands (depending on opcode).
     */
    int getNumOperands() const
    {
        if (!opcodeInfo) {
            return 0;
        }
        return opcodeInfo->numOperands;
    }

    /*
     * Get all explicit operands.
     */
    const ExplicitStaticOperands &getOperands() const
    {
            return rawOperands;
    }

    /*
     * Get the explicit operand at the specified index.
     */
    const StaticOperand &getOperand(int idx) const
    {
        drob_assert(idx >= 0 && idx < ARCH_MAX_OPERANDS);

        return rawOperands.op[idx];
    }

    /*
     * Set the operand at the specified index;
     */
    void setOperand(int idx, StaticOperand &operand)
    {
        drob_assert(idx >= 0 && idx < ARCH_MAX_OPERANDS);

        modified();
        rawOperands.op[idx] = operand;
    }

    /*
     * Return detailed information about this opcode that does not
     * depend on the actual operands.
     */
    const OpcodeInfo *getOpcodeInfo(void)
    {
        return opcodeInfo;
    }

    const ExplicitStaticOperandInfo *getOperandInfo(int idx) const
    {
        drob_assert(idx >= 0 && idx < ARCH_MAX_OPERANDS);

        if (!opcodeInfo)
            return nullptr;

        return &opcodeInfo->opInfo[idx];
    }

    /*
     * Reset the wole instruction to completely reconstruct it (keeping
     * the original instruction information).
     */
    void clear(void)
    {
        modified();
        opcode = Opcode::NONE;
        opcodeInfo = nullptr;
        /* FIXME: clear/invalidate operands */
    }

    /*
     * Return the predicate (if any). Without a predicate, the
     * instruction will always execute (or the predicate cannot be modeled).
     */
    const Predicate *getPredicate(void) const
    {
        if (!opcodeInfo) {
            return nullptr;
        }
        return opcodeInfo->predicate;
    }

    /*
     * Calculate and cache information about this instruction without
     * a given ProgramState. So it contains only information valid
     * under any ProgramState.
     */
    const InstructionInfo& getInfo(void) const;

    /*
     * Return calculated information about this instruction cached during
     * last emulate call.
     */
    const DynamicInstructionInfo *getDynInfo(void) const;

    /*
     * Invalidate and remove the dynamic instruction info.
     */
    void clearDynInfo(void)
    {
        cachedDynInfo.reset();
    }

    /*
     * If this instruction is based on an original instruction part of
     * the function to be rewritten, the begin of that instruction.
     * Otherwise 0.
     */
    const uint8_t *getStartAddr(void) const
    {
        return itext;
    }

    /*
     * If this instruction is based on an original instruction part of
     * the function to be rewritten, the inclusive end of that instruction.
     * Otherwise 0.
     */
    const uint8_t *getEndAddr(void) const
    {
        if (!itext)
            return 0;
        return itext + ilen - 1;
    }

    /*
     * Get the branch edge for a branch instruction.
     */
    const std::shared_ptr<BranchEdge>& getBranchEdge(void) const
    {
        drob_assert(isBranch());
        return branchEdge;
    }

    /*
     * Set the branch edge for a branch instruction.
     */
    void setBranchEdge(const std::shared_ptr<BranchEdge>& edge)
    {
        drob_assert(isBranch());
        this->branchEdge = edge;
    }

    /*
     * Get the return edge for a return instruction.
     */
    const std::shared_ptr<ReturnEdge>& getReturnEdge(void) const
    {
        drob_assert(isRet());
        return returnEdge;
    }

    /*
     * Set the return edge for a return instruction.
     */
    void setReturnEdge(const std::shared_ptr<ReturnEdge>& edge)
    {
        drob_assert(isRet());
        this->returnEdge = edge;
    }

    bool getUseShortBranch(void) const
    {
        drob_assert(isBranch());
        return useShortBranch;
    }

    void setUseShortBranch(bool useShortBranch)
    {
        drob_assert(isBranch());
        this->useShortBranch = useShortBranch;
    }

    /*
     * Get the call edge for a call instruction.
     */
    const std::shared_ptr<CallEdge>& getCallEdge(void) const
    {
        drob_assert(isCall());
        return callEdge;
    }

    /*
     * Set the call edge for a call instruction.
     */
    void setCallEdge(const std::shared_ptr<CallEdge>& edge)
    {
        drob_assert(isCall());
        this->callEdge = edge;
    }

    /*
     * Is this a return instruction?
     */
    bool isRet(void) const
    {
        if (!opcodeInfo)
            return false;
        return opcodeInfo->type == OpcodeType::Ret;
    }

    /*
     * Is this a call instruction?
     */
    bool isCall(void) const
    {
        if (!opcodeInfo)
            return false;
        return opcodeInfo->type == OpcodeType::Call;
    }

    /*
     * Is this a branch instruction?
     */
    bool isBranch(void) const
    {
        if (!opcodeInfo)
            return false;
        return opcodeInfo->type == OpcodeType::Branch;
    }

    /*
     * Get the raw target (original itext) for a call or branch.
     * Return nullptr if it cannot be detected.
     */
    const uint8_t *getRawTarget(const MemProtCache &memProtCache) const;

    /*
     * Generate code. (excluding handling branches/calls with known
     * targets).
     */
    const uint8_t *generateCode(BinaryPool &binaryPool, bool write);

    /*
     * Emulate the instruction on the given ProgramState.
     */
    void emulate(ProgramState &ps, const RewriterCfg &cfg,
                 const MemProtCache &memProtCache, bool cacheDynInfo) const;

    /*
     * Given the ProgramState, do we know if the instruction
     * (with predicates) will execute?
     */
    TriState willExecute(const ProgramState &ps) const;

    /*
     * Set the register liveness info (register liveness analysis).
     */
    void setLivenessData(std::unique_ptr<LivenessData> irl)
    {
        liveRegs = std::move(irl);
    }
    /*
     * Get the register liveness info (register liveness analysis).
     */
    LivenessData* getLivenessData(void)
    {
        return liveRegs.get();
    }

    /*
     * Dump this instruction to stderr.
     */
    void dump(void) const;
private:
    void modified()
    {
        reencode = true;
        cachedInfo.reset();
        cachedDynInfo.reset();
        liveRegs.reset();
    }

    /*
     * Original instruction text this instruction is based on (original,
     * modified, inserted) - necessary for block splitting.
     */
    const uint8_t *itext;
    uint8_t ilen;

    /*
     * Will we have to generate code or can the original itext be reused?
     */
    bool reencode;

    /*
     * Instruction details either parsed from an original instruction
     * or modified.
     */
    Opcode opcode;
    ExplicitStaticOperands rawOperands;

    /*
     * Opcode information cached when switching the opcode.
     */
    const OpcodeInfo *opcodeInfo;

    /*
     * Call/Branch/Ret specifics.
     */
    std::shared_ptr<BranchEdge> branchEdge;
    std::shared_ptr<CallEdge> callEdge;
    std::shared_ptr<ReturnEdge> returnEdge;
    bool useShortBranch{false};

    /*
     * Cached information about this instruction. Invalidated on
     * changes.
     */
    mutable std::unique_ptr<InstructionInfo> cachedInfo;
    mutable std::unique_ptr<DynamicInstructionInfo> cachedDynInfo;
    mutable std::unique_ptr<LivenessData> liveRegs;

    /* Helper functions */
    OperandInfo createOperandInfo(const StaticOperandInfo &rawInfo, int nr,
                                  bool isImpl) const;
    DynamicOperandInfo createDynOpInfo(const OperandInfo &operand,
                                       const ProgramState &ps,
                                       const RewriterCfg &cfg,
                                       const MemProtCache &memProtCache) const;
    std::unique_ptr<DynamicInstructionInfo> genDynInfo(
            ProgramState &ps, const RewriterCfg &cfg,
            const MemProtCache &memProtCache) const;
};

} /* namespace drob */

#endif /* INSTRUCTION_HPP */
