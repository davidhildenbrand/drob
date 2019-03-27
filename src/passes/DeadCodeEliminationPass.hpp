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
#ifndef PASSES_DEAD_CODE_ELIMINATION_PASS_HPP
#define PASSES_DEAD_CODE_ELIMINATION_PASS_HPP

#include <vector>
#include "../Utils.hpp"
#include "../Pass.hpp"
#include "../NodeCallback.hpp"
#include "../Instruction.hpp"

namespace drob {

/*
 * Dead Code Elimination will remove
 * - Instructions/blocks/functions that can never be reached
 * - Predicated instructions whose predicate always evaluates to false
 * - Instructions with only dead register writes (->DeadRegisterWriteElimination)
 * Also
 * - Conditional branches will be converted to unconditional branches
 *
 * Stack analysis is required (-> implicit Register Liveness Analysis). We
 * exploit the fact that dead instructions will not get stack analysis data
 * attached.
 *
 * Usually, this pass should be run early, so we have a completely clean stack
 * analysis.
 */
class DeadCodeEliminationPass: public Pass, public NodeCallback {
public:
    DeadCodeEliminationPass(ICFG &icfg, BinaryPool &binaryPool,
                        const RewriterCfg &cfg,
                        const MemProtCache &memProtCache) :
            Pass(icfg, binaryPool, cfg, memProtCache, "DeadCodeRemoval",
                 "Remove dead code")
    {
    }

    /*
     * When removing functions, contained blocks and instructions are removed.
     * When removing blocks, contained instructions are removed.
     * When removing instructions, edges are removed (also from the relevant
     * blocks/functions).
     */
    bool run(void)
    {
        /* collect functions, blocks and instructions that are unreachable */
        icfg.for_each_function_any(this);

        /* Remove all instruction */
        for (auto &i : instructionsToDelete) {
            i.second->removeInstruction(i.first);
        }
        instructionsToDelete.resize(0);

        /* Convert conditional branches to unconditional branches */
        for (auto &i : condBranchesToConvert) {
            SuperBlock *block = i.second;
            Instruction *instruction = i.first;

            drob_info("Replacing conditional by unconditional branch: %p (%p)",
                      instruction, instruction->getStartAddr());
            /* invalidate the fallthrough chain */
            if (block->getNext()) {
                block->getNext()->setPrev(nullptr);
                block->setNext(nullptr);
            }
            /* FIXME: architecture callback -> this is some sort of specialization ? */
            instruction->setOpcode(Opcode::JMPa);

            /* TODO: replacing operands should maybe happen via the superblock */
            block->invalidateStackAnalysis();
            block->invalidateLivenessAnalysis();
        }
        condBranchesToConvert.resize(0);

        /* Remove all blocks */
        for (auto &b : blocksToDelete) {
            b.second->removeBlock(b.first);
        }
        blocksToDelete.resize(0);

        /* Remove all functions */
        for (auto &f : functionsToDelete) {
            icfg.removeFunction(f);
        }
        functionsToDelete.resize(0);

        return false;
    }

    bool needsStackAnalysis(void)
    {
        /* We need a valid stack analysis */
        return true;
    }

    int handleFunction(Function *function)
    {
        /* FIXME: analyze all functions. For now we have to skip these */
        if (!function->getEntryBlock()->getEntryState()) {
            return 0;
        }
        /* No state attached during stack analysis -> unreachable */
        if (!function->getEntryBlock()->getEntryState()) {
            functionsToDelete.push_back(function);
            return 0;
        }
        function->for_each_block_any(this);
        return 0;
    }

    int handleBlock(SuperBlock *block, Function *function)
    {
        /* No state attached during stack analysis -> unreachable */
        if (!block->getEntryState()) {
            blocksToDelete.push_back(std::make_pair(block, function));
            return 0;
        }
        block->for_each_instruction(this, function);
        return 0;
    }

    int handleInstruction(Instruction *instruction, SuperBlock *block,
                          Function *function)
    {
        const DynamicInstructionInfo *dynInfo = instruction->getDynInfo();
        (void)block;
        (void)function;

        /* No state attached during stack analysis -> unreachable or dead */
        if (!dynInfo) {
            drob_debug("Instruction has no stack analysis data: %p (%p)",
                      instruction, instruction->getStartAddr());
            instructionsToDelete.push_back(std::make_pair(instruction, block));
            return 0;
        }

        /* Instruction that will never get executed */
        if (dynInfo->willExecute == TriState::False) {
            drob_debug("Instruction predicate is always false: %p (%p)",
                      instruction, instruction->getStartAddr());
            instructionsToDelete.push_back(std::make_pair(instruction, block));
            return 0;
        }

        /*
         * Convert conditional branches to unconditional branches, to make
         * sure removing following instructions doesn't break anything.
         */
        if (instruction->isBranch() && instruction->getPredicate() &&
            dynInfo->willExecute == TriState::True) {
            condBranchesToConvert.push_back(std::make_pair(instruction, block));
            return 0;
        }
        return 0;
    }

    std::vector<Function *> functionsToDelete;
    std::vector<std::pair<SuperBlock *, Function*>> blocksToDelete;
    std::vector<std::pair<Instruction *, SuperBlock *>> instructionsToDelete;
    std::vector<std::pair<Instruction *, SuperBlock *>> condBranchesToConvert;
};

} /* namespace drob */

#endif /* PASSES_DEAD_CODE_ELIMINATION_PASS_HPP */
