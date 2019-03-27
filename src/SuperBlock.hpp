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
#ifndef SUPERBLOCK_HPP
#define SUPERBLOCK_HPP

#include <list>
#include <vector>
#include <memory>
#include <queue>
#include <algorithm>

#include "Utils.hpp"
#include "Instruction.hpp"
#include "Node.hpp"
#include "NodeCallback.hpp"

namespace drob {

class Rewriter;
class BinaryPool;
class SuperBlock;
class Function;
class ProgramState;

typedef struct BranchEdge {
    BranchEdge(SuperBlock *dst, SuperBlock *src, Instruction *instruction) :
        dst(dst), src(src), instruction(instruction) {}
    BranchEdge() :
        dst(nullptr), src(nullptr), instruction(nullptr) {}
    BranchEdge(const BranchEdge& rhs)
    {
        dst = rhs.dst;
        src = rhs.src;
        instruction = rhs.instruction;
        invalidated = rhs.invalidated;
    }

    SuperBlock *dst;
    SuperBlock *src;
    Instruction *instruction;
    bool invalidated{false};

    void invalidate(void);
} BlockEdge;

/*
 * A SuperBlock is single-entry, multiple-exit.
 */
class SuperBlock : public Node {
public:
    SuperBlock(Function *function) : Node((Node *)function), function(function) {}

    const std::list<std::unique_ptr<Instruction>>& getInstructions(void)
    {
        return instrs;
    }

    void addInstructions(std::list<std::unique_ptr<Instruction>> &addedInstrs)
    {
        instrs.splice(instrs.end(), addedInstrs);
    }

    void appendInstruction(std::unique_ptr<Instruction> &instr)
    {
        instrs.push_back(std::move(instr));
    }

    /*
     * Return the original start address of the first instruction of the block.
     * If there are no instructions or if the first instruction has no original
     * instruction, returns nullptr.
     */
    const uint8_t *getStartAddr(void) const
    {
        if (!instrs.empty()) {
            return instrs.front()->getStartAddr();
        }
        return nullptr;
    }

    /*
     * Return the original end address of the last instruction of the block.
     * If there are no instructions or if the last instruction has no original
     * instruction, returns nullptr.
     */
    const uint8_t *getEndAddr(void) const
    {
        if (!instrs.empty()) {
            return instrs.back()->getEndAddr();
        }
        return nullptr;
    }

    /*
     * Go over all instructions in sequential order.
     */
    int for_each_instruction(NodeCallback *cb, Function *function)
    {
        int ret = 0;

        for (auto && instr : instrs) {
            ret = cb->handleInstruction(instr.get(), this, function);
            if (ret)
                break;
        }
        return ret;
    }


    /*
     * Go over all instructions in reverse sequential order, starting with
     * the given instruction.
     */
    int for_each_instruction_rev(NodeCallback *cb, Function *function,
                                 Instruction *start)
    {
        int ret = 0;

        /* skip all instructions from the end until we found the start */
        auto it = instrs.rbegin();
        for (; it != instrs.rend() && it->get() != start; ++it)
            ;
        for (; it != instrs.rend(); ++it) {
            ret = cb->handleInstruction(it->get(), this, function);
            if (ret)
                break;
        }
        return ret;
    }

    SuperBlock* getNext(void)
    {
        return next;
    }

    const SuperBlock* getNext(void) const
    {
        return next;
    }

    void setNext(SuperBlock *next)
    {
        this->next = next;
    }

    SuperBlock* getPrev(void)
    {
        return prev;
    }

    const SuperBlock* getPrev(void) const
    {
        return prev;
    }

    void setPrev(SuperBlock *prev)
    {
        this->prev = prev;
    }

    void setEntryState(std::unique_ptr<ProgramState> state)
    {
        entryState = std::move(state);
    }

    ProgramState *getEntryState(void)
    {
        return entryState.get();
    }

    void setLivenessData(std::unique_ptr<LivenessData> liveRegs)
    {
        livenessData = std::move(liveRegs);
    }

    LivenessData* getLivenessData(void)
    {
        return livenessData.get();
    }

    /*
     * Delete an instruction from the block, along with edges.
     */
    void removeInstruction(Instruction *instruction);

    /*
     * Delete all instruction from the block, along with edges.
     */
    void removeAllInstructions(void);

    const std::vector<std::shared_ptr<BranchEdge>>& getIncomingEdges(void) const
    {
        return incomingEdges;
    }

    void addIncomingEdge(const std::shared_ptr<BranchEdge> &edge)
    {
        invalidateStackAnalysis();
        drob_assert(edge->dst == this);
        incomingEdges.push_back(edge);
    }

    void removeIncomingEdge(const BranchEdge* edge)
    {
        invalidateStackAnalysis();
        for (auto it = incomingEdges.begin(); it != incomingEdges.end(); it++) {
            if (it->get() == edge) {
                incomingEdges.erase(it);
                return;
            }
        }
        drob_assert_not_reached();
    }

    const std::vector<std::shared_ptr<BranchEdge>>& getOutgoingEdges(void) const
    {
        return outgoingEdges;
    }

    void addOutgoingEdge(const std::shared_ptr<BranchEdge> &edge)
    {
        invalidateLivenessAnalysis();
        drob_assert(edge->src == this);
        outgoingEdges.push_back(edge);
    }

    void removeOutgoingEdge(const BranchEdge* edge)
    {
        invalidateLivenessAnalysis();
        for (auto it = outgoingEdges.begin(); it != outgoingEdges.end(); it++) {
            if (it->get() == edge) {
                outgoingEdges.erase(it);
                return;
            }
        }
        drob_assert_not_reached();
    }

    /*
     * Replace ->next by an explicit jump.
     */
    void unchainNext(void)
    {
        if (likely(next != nullptr)) {
            ExplicitStaticOperands dummy = {};

            /* Fake any address, we have an edge, so this is basically ignored */
            dummy.op[0].mem.type = MemPtrType::Direct;
            std::unique_ptr<Instruction> newBranch = std::make_unique<Instruction>(Opcode::JMPa,
                                                                                   dummy);
            std::shared_ptr<BranchEdge> newEdge = std::make_shared<BranchEdge>(next, this, newBranch.get());

            newBranch->setBranchEdge(newEdge);
            appendInstruction(newBranch);
            addOutgoingEdge(newEdge);
            next->addIncomingEdge(newEdge);
            next->prev = nullptr;
            next = nullptr;
        }
    }

    Function *getFunction(void)
    {
        return function;
    }
private:
    friend class Function;

    /* the parent function */
    Function *function;

    /* list of all instructions contained in this block */
    std::list<std::unique_ptr<Instruction>> instrs;

    /* next block in the fallthrough chain */
    SuperBlock *next{nullptr};

    /* previous fallthrough block in the fallthough chain */
    SuperBlock *prev{nullptr};

    /* incoming edges (excluding prev) */
    std::vector<std::shared_ptr<BranchEdge>> incomingEdges;

    /* outgoing edges (excluding next) */
    std::vector<std::shared_ptr<BranchEdge>> outgoingEdges;

    /* attached entry ProgramState */
    std::unique_ptr<ProgramState> entryState;
    /* attached register liveness data */
    std::unique_ptr<LivenessData> livenessData;
};

} /* namespace drob */

#endif /* SUPERBLOCK_HPP */
