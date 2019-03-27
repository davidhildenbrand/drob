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
#ifndef PASSES_STACK_ANALYSIS_PASS_HPP
#define PASSES_STACK_ANALYSIS_PASS_HPP

#include <queue>
#include "../Rewriter.hpp"
#include "../Utils.hpp"
#include "../Pass.hpp"
#include "../NodeCallback.hpp"
#include "../Instruction.hpp"

namespace drob {

class StackAnalysisPass : public Pass, public NodeCallback {
public:
    StackAnalysisPass(ICFG &icfg, BinaryPool &binaryPool,
                      const RewriterCfg &cfg, const MemProtCache &memProtCache) :
            Pass(icfg, binaryPool, cfg, memProtCache, "StackAnalysis",
                 "Full stack analysis and constant propagation")
    {
    }


    bool needsLivenessAnalysis(void)
    {
        /*
         * We need don't want to propagate never read register values through
         * the ProgramState, eventually making delta stack analysis very
         * expensive. We need to know for each instruction if registers will
         * eventually be read - before doing the stack analysis.
         */
        return true;
    }

    void forwardStateToBlock(SuperBlock *block, const ProgramState *state) {
        drob_info("Forwarding state to block %p (%p)", block,
                   block->getStartAddr());

        if (block->getEntryState()) {
            /*
             * We have to merge both states. If we detect a change, we have to
             * reanalyze that block using the new, merged state.
             */
            drob_debug("Merging of states required");
            //curState->dump();
            //block->getEntryState()->dump();
            if (block->getEntryState()->merge(*curState)) {
                drob_debug("Change detected during merge");
                block->stackAnalysisValid = false;
                if (!block->queued) {
                    block->queued = true;
                    blocksToProcess.push(block);
                }
            } else {
                drob_debug("No change detected during merge");
            }
        } else {
            drob_debug("No merging of states required");
            /*
             * Copy the state and remember to follow that path.
             */
            block->setEntryState(std::make_unique<ProgramState>(*state));
            block->stackAnalysisValid = false;
            if (!block->queued) {
                block->queued = true;
                blocksToProcess.push(block);
            }
        }
    }

    void processBlock(SuperBlock *block, Function *function)
    {
        if (block->stackAnalysisValid) {
            drob_debug("Block %p (%p) has a valid analysis", block, block->getStartAddr());
            return;
        }

        /*
         * Set the state to valid now. Might be that this block will never
         * get executed.
         */
        block->stackAnalysisValid = true;

        /*
         * No entry state -> we have to recompute all entries + prev. They
         * have to push the state into this block. If these blocks don't
         * have an entry state, it means they might not be executed at all.
         *
         * Case 1: They will be executed. They are already queue and once
         * they are analyzed, this block will be updated and re-enqueued.
         * Case 2: They won't be executed. When trying to analyze them,
         * we will play the same game as with this block.
         */
        if (!block->getEntryState()) {
            SuperBlock *prev = block->getPrev();

            if (prev && !prev->queued && prev->getEntryState()) {
                prev->stackAnalysisValid = false;
                prev->queued = true;
                blocksToProcess.push(prev);
            }
            for (auto & edge : block->getIncomingEdges()) {
                prev = edge->src;
                if (!prev->queued && prev->getEntryState()) {
                    prev->stackAnalysisValid = false;
                    prev->queued = true;
                    blocksToProcess.push(prev);
                }
            }
            return;
        }

        drob_info("Analyzing block: %p (%p)", block,
                  block->getStartAddr());

        /*
         * Copy the state so we can push it through the block.
         */
        drob_assert(block->getEntryState());
        ProgramState state = ProgramState(*block->getEntryState());

        curState = &state;
        if (!block->for_each_instruction(this, function) &&
            block->getNext()) {
            /* fall through block will be executed, process it */
            forwardStateToBlock(block->getNext(), curState);
        }
        curState = nullptr;
    }

    int handleBlock(SuperBlock *block, Function *function)
    {
        (void)function;
        if (!block->queued && !block->stackAnalysisValid) {
            block->queued = true;
            blocksToProcess.push(block);
        }
        return 0;
    }

    void processFunction(Function *function)
    {
        if (function->stackAnalysisValid)
            return;

        drob_info("Analyzing function %p (%p)", function,
                  function->getStartAddr());

        /*
         * Enqueue all blocks that need a valid analysis and are not
         * yet enqueued. Use BFS for now, this will enqueue the entry block
         * first and the others in a reasonable order.
         */
        function->for_each_block_bfs(this);

        while (!blocksToProcess.empty()) {
            auto & block = blocksToProcess.front();
            blocksToProcess.pop();
            block->queued = false;

            drob_debug("Dequeuing block %p (%p", block, block->getStartAddr());

            processBlock(block, function);
        }

        function->stackAnalysisValid = true;
    }

    bool run(void)
    {
        Function *entryFunction = icfg.getEntryFunction();
        SuperBlock *entryBlock;

        if (icfg.stackAnalysisValid || unlikely(!entryFunction))
            return false;
        entryBlock = entryFunction->getEntryBlock();
        if (unlikely(!entryBlock))
            return false;

        /*
         * Configure the entry state for the entry block, which is the starting
         * point of our analysis.
         */
        if (unlikely(!entryBlock->getEntryState())) {
            entryBlock->setEntryState(std::make_unique<ProgramState>(cfg.getEntryState()));
        }

        /*
         * TODO for now, we only handle the entry function for which we
         * don't allow recursion, so we can safely optimize it.
         */
        processFunction(entryFunction);

        icfg.stackAnalysisValid = true;
        return false;
    }

    static bool regIsDead(const LivenessData *livenessData, Register reg,
                          RegisterAccessType type = RegisterAccessType::Full)
    {
        return !(livenessData->live_out & getSubRegisterMask(reg, type));
    }

    int handleInstruction(Instruction *instruction, SuperBlock *block,
                  Function *function)
    {
        const LivenessData *livenessData = instruction->getLivenessData();
        const InstructionInfo& info = instruction->getInfo();
        (void)block;
        (void)function;

        drob_info("Analyzing instruction: %p (%p)", instruction,
                  instruction->getStartAddr());

        /*
         * Stack Analysis Optimization:
         *
         * If liveness analysis didn't attach liveness information, this instruction
         * can essentially be dropped later, no outputs are relevant. No need
         * to emulate. No liveness analysis data -> no stack analysis data.
         */
        if (!livenessData) {
            drob_debug("Instruction has no liveness analysis data");
            return 0;
        }

        //curState->dump();
        //instruction->dump();

        /*
         * Emulate the instruction on the program state.
         */
        drob_debug("Calculating stack analysis data");
        instruction->emulate(*curState, cfg, memProtCache, true);

        /*
         * Stack Analysis Optimization:
         *
         * Now that all values have been read/written in the program state, set
         * DEAD registers to DEAD: If nobody will be reading a value from a
         * register we would write, or we are the last one to read it, set set
         * it to DEAD. Avoid propagating dead values through the program. This
         * is a heavy performance improvement especially for delta stack
         * analysis. No need to propagate values too far.
         */
        if (unlikely(instruction->getPredicate() != nullptr)) {
            const Predicate *predicate = instruction->getPredicate();

            for (int i = 0; i < 2; i++) {
                const PredComparison &comp = predicate->comparisons[i];

                if (!comp.lhs.isImm && regIsDead(livenessData, comp.lhs.reg)) {
                    curState->setRegister(comp.lhs.reg, DataType::Dead);
                }
                if (!comp.rhs.isImm && regIsDead(livenessData, comp.rhs.reg)) {
                    curState->setRegister(comp.rhs.reg, DataType::Dead);
                }
                if (comp.con == PredConjunction::None) {
                    break;
                }
            }
        }
        for (auto &op : info.operands) {
            if (op.type == OperandType::Register) {
                if (isRead(op.r.mode) && regIsDead(livenessData, op.r.reg, op.r.r)) {
                    curState->setRegister(op.r.reg, op.r.r, DataType::Dead);
                }
                if (isWrite(op.r.mode) && regIsDead(livenessData, op.r.reg, op.r.w)) {
                    curState->setRegister(op.r.reg, op.r.w, DataType::Dead);
                }
            } else if (op.type == OperandType::MemPtr &&
                       op.m.ptr.type == MemPtrType::SIB) {
                if (op.m.ptr.sib.base != Register::None &&
                    regIsDead(livenessData, op.m.ptr.sib.base)) {
                    curState->setRegister(op.m.ptr.sib.base, DataType::Dead);
                }
                if (op.m.ptr.sib.index != Register::None &&
                    regIsDead(livenessData, op.m.ptr.sib.index)) {
                    curState->setRegister(op.m.ptr.sib.index, DataType::Dead);
                }
            }
        }

        //dump(*instruction->getDynInfo());
        if (instruction->isCall()) {
            /*
             * TODO: for now called functions are not analyzed. They could do
             * anything.
             */
            curState->nastyInstruction();
        } else if (instruction->isBranch() && instruction->getBranchEdge()) {
            /* only follow branches that will be executed */
            switch (instruction->getDynInfo()->willExecute) {
            case TriState::True:
                forwardStateToBlock(instruction->getBranchEdge()->dst, curState);
                /* Instructions following this branch will not be executed */
                return 1;
            case TriState::Unknown:
                forwardStateToBlock(instruction->getBranchEdge()->dst, curState);
                break;
            default:
                break;
            }
        }
        return 0;
    }
private:
    ProgramState *curState;
    std::queue<SuperBlock *> blocksToProcess;
};

} /* namespace drob */

#endif /* PASSES_STACK_ANALYSIS_PASS_HPP */
