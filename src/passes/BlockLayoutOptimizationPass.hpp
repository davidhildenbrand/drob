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
#ifndef PASSES_BLOCK_LAYOUT_OPTIMIZATION_PASS_HPP
#define PASSES_BLOCK_LAYOUT_OPTIMIZATION_PASS_HPP

#include <vector>
#include "../Utils.hpp"
#include "../Pass.hpp"
#include "../NodeCallback.hpp"
#include "../Instruction.hpp"

namespace drob {

class BlockLayoutOptimizationPass: public Pass, public NodeCallback {
public:
    BlockLayoutOptimizationPass(ICFG &icfg, BinaryPool &binaryPool,
                        const RewriterCfg &cfg,
                        const MemProtCache &memProtCache) :
            Pass(icfg, binaryPool, cfg, memProtCache, "JumpOptimization",
                 "Optimize jumps and block layout")
    {
    }

    bool run(void)
    {
        /* collect blocks to merge and directly chain blocks */
        icfg.for_each_block_any(this);

        /* merge the desired blocks */
        for (auto && b : blocksToMerge) {
            b.second->mergeBlockIntoNext(b.first);
        }
        blocksToMerge.clear();
        return false;
    }

    int handleBlock(SuperBlock *block, Function *function)
    {
        drob_info("Handling block: %p (%p)", block, block->getStartAddr());
        /* empty block -> merge into successor */
        if (block->getInstructions().empty()) {
            drob_assert(block->getNext());
            blocksToMerge.push_back(std::make_pair(block, function));
            return 0;
        } else if (!block->getNext()) {
            /* possible chain */
            const std::unique_ptr<Instruction> &lastInstr = block->getInstructions().back();

            /* last instruction has to be an unconditional branch with known edge */
            if (!lastInstr->isBranch() || lastInstr->getPredicate() || !lastInstr->getBranchEdge()) {
                return 0;
            }

            /* the destination must not already be part of a chain */
            BranchEdge *edge = lastInstr->getBranchEdge().get();
            SuperBlock *dst = edge->dst;
            if (dst->getPrev()) {
                return 0;
            }

            drob_info("Chaining blocks: %p (%p) and  %p (%p)", block,
                      block->getStartAddr(), dst, dst->getStartAddr(), dst);
            /* wire up the chain, dropping the jump */
            dst->setPrev(block);
            block->setNext(dst);
            /* remove the jump along with the edge */
            block->removeInstruction(lastInstr.get());
        }

        /* Try to merge chained blocks */
        if (block->getNext() && block->getNext()->getIncomingEdges().empty()) {
            blocksToMerge.push_back(std::make_pair(block, function));
        }

        return 0;
    }
    std::vector<std::pair<SuperBlock *, Function*>> blocksToMerge;
};

} /* namespace drob */

#endif /* PASSES_BLOCK_LAYOUT_OPTIMIZATION_PASS_HPP */
