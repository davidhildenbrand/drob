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
#ifndef PASSES_SIMPLE_LOOP_UNROLLING_PASS_HPP
#define PASSES_SIMPLE_LOOP_UNROLLING_PASS_HPP

#include "../Pass.hpp"

namespace drob {

/*
 * Loop unrolling of loops that consist only of a single block. Loops are
 * unrolled always 10 times. We can deal with block that have more than one
 * self-branch.
 *
 * Optimizations for the future:
 * - Detect loop condition and use a better heuristic
 * - Perform dead code removal before and after unrolling, not just after the
 *   unrolling. This avoids unrolling loops that are completely optimized out
 *   or only need one iteration.
 * - Perform a second loop unrolling after dead code elimination and
 *   jump optimization. Might allow to unroll nested loops completely.
 */
class SimpleLoopUnrollingPass : public Pass, public NodeCallback {
public:
    SimpleLoopUnrollingPass(ICFG &icfg, BinaryPool &binaryPool,
               const RewriterCfg &cfg, const MemProtCache &memProtCache) :
        Pass(icfg, binaryPool, cfg, memProtCache, "SimpleLoopUnrolling", "Very simple loop unrolling") {}

    void unrollSingleBlockLoop(SuperBlock *block, Function *function)
    {
        Instruction *lastSelfBranchInstr = nullptr;

        (void)function;
        (void)block;

        drob_info("Unrolling block %p (%p)", block, block->getStartAddr());

        /* Find the last self-branch. */
        for (auto it = block->getOutgoingEdges().rbegin();
             it != block->getOutgoingEdges().rend(); it++) {
            auto &edge = *it;
            if (edge->dst == block) {
                lastSelfBranchInstr = edge->instruction;
                break;
            }
        }

        /*
         * Split after the last self-branch if necessary.
         */
        drob_assert(lastSelfBranchInstr);
        if (lastSelfBranchInstr != block->getInstructions().back().get()) {
            function->splitBlockAfter(block, lastSelfBranchInstr);
        }

        /*
         * Replace block->next by an explicit branch. This makes it way easier
         * top copy blocks and wire them up. Will be optimized out later on.
         */
        if (block->getNext()) {
            block->unchainNext();
            drob_assert(!block->getNext());
        }

        for (uint16_t i = 0; i < cfg.getDrobCfg().simple_loop_unroll_count; i++) {
            SuperBlock *copy;

            /* Copy the block. This will correctly wire up all outgoing edges. */
            copy = function->copyBlock(block);

            /*
             * We will place the copy after the original block. This way, we don't
             * have to modify incoming edges. Point all self-branch edges of the
             * original block at our copy.
             */
            for (auto & edge : block->getOutgoingEdges()) {
                if (edge->dst == block) {
                    edge->dst->removeIncomingEdge(edge.get());
                    edge->dst = copy;
                    edge->dst->addIncomingEdge(edge);
                }
            }

            /*
             * We now might have the original block look something like:
             *  jcc COPY
             *  jmp LOOP_END
             * This won't let us chain the unrolled iterations. We actually
             * want
             *  jcc LOOP_END
             *  + chain to COPY
             *  In the ideal case, the jcc can be optimized out later.
             */
            Instruction *lastInstruction = block->getInstructions().back().get();
            if (lastInstruction->getBranchEdge()->dst != copy) {
                drob_assert(!lastInstruction->getPredicate());
                /* find the conditional branch instruction, try to invert the condition */
                auto it = block->getInstructions().rbegin();
                std::advance(it, 1);
                Opcode opcode = arch_invert_branch(it->get()->getOpcode());

                /* we can actually invert the branch condition */
                if (opcode != Opcode::NONE) {
                    /* drop the conditional branch */
                    block->removeInstruction(it->get());

                    /* change the opcode of the jump to the inverted branch condition */
                    lastInstruction->setOpcode((opcode));

                    /* install the chaining */
                    drob_assert(!copy->getPrev());
                    block->setNext(copy);
                    copy->setPrev(block);
                }
            }
            /* continue unrolling with our copy */
            block = copy;
        }
    }

    int handleBlock(SuperBlock *block, Function *function)
    {
        (void)function;
        for (auto && edge : block->getOutgoingEdges()) {
            if (edge->dst == block) {
                blocksToProcess.push_back(std::make_pair(block, function));

                /*
                 * Stop here, we might have multiple back edges to the entry
                 * in a block, process each block only once.
                 */
                break;
            }
        }
        return 0;
    }

    bool run(void)
    {
        if (unlikely(!cfg.getDrobCfg().simple_loop_unroll_count))
            return false;

        /* Collect simple loops */
        icfg.for_each_block_any(this);

        /* Unroll the collected, simple loops */
        for (auto &&entry : blocksToProcess) {
            unrollSingleBlockLoop(entry.first, entry.second);
        }
        return false;
    }

private:
    std::vector<std::pair<SuperBlock *, Function *>> blocksToProcess;
};

} /* namespace drob */

#endif /* PASSES_SIMPLE_LOOP_UNROLLING_PASS_HPP */
