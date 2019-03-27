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
#ifndef PASSES_ICFG_RECONSTRUCTION_PASS_HPP
#define PASSES_ICFG_RECONSTRUCTION_PASS_HPP

#include "../Utils.hpp"
#include "../Pass.hpp"

namespace drob {

/*
 * ICFG reconstruction tries to minimize duplicate code in the ICFG. E.g.
 * blocks are split instead of decoding a new one.
 *
 * There are two cases, where duplicate code will be part of the ICFG
 *
 * 1. Branching to parts of an instruction. not to the start (e.g. jump over
 *    prefixes)
 * 2. When blocks would have to be split, if the jumps are processed in reverse
 *    order - when decoding the second part of the block, we are not aware of the
 *    first part of the block. While decoding the first part of the block, we
 *    would have to check after every instruction if we already have a block
 *    starting at that instruction. Can be done while decoding, or after ICFG
 *    reconstruction.
 */

class BranchEdgeCollector: public NodeCallback {
    int handleInstruction(Instruction *instruction, SuperBlock *block,
                          Function *function)
    {
        (void)function;
        if (instruction->isBranch() && !instruction->getBranchEdge()) {
            BranchEdge edge = BranchEdge(nullptr, block, instruction);

            edges.push_back(edge);
        }
        return 0;
    }
public:
    std::vector<BranchEdge> edges;
};

class CallEdgeCollector: public NodeCallback {
    int handleInstruction(Instruction *instruction, SuperBlock *block,
                          Function *function)
    {
        (void)block;
        if (instruction->isCall() && !instruction->getCallEdge()) {
            CallEdge edge = { .dst = nullptr, .src = function, .instruction =
                    instruction, };

            edges.push_back(edge);
        }
        return 0;
    }
public:
    std::vector<CallEdge> edges;
};

/*
 * Check if the itext is already covered by any of the blocks given in blockMap.
 * If so,
 */
static bool iTextCovered(std::unordered_map<const uint8_t *, SuperBlock *> &blockMap,
                         const uint8_t *itext, SuperBlock **blockToSplit,
                         Instruction **instructionToSplitAt)
{
    /*
     * TODO: We might not want to split at all or make it configurable in the
     * future. If we don't split, most loops are implicitly unrolled by one,
     * allowing us to eventually propagate constants.
     */
    for (auto block : blockMap) {
        const uint8_t *start = block.first;
        const uint8_t *end = block.second->getEndAddr();

        /* we should never get called if it is the start of a known block */
        drob_assert(start != itext);

        /* fast check if the instruction is covered at all */
        if (!start || !end || itext < start || itext > end)
            continue;

        /* find the instruction (special case: overlying instructions!) */
        for (auto & instr : block.second->getInstructions()) {
            if (instr->getStartAddr() == itext) {
                *blockToSplit = block.second;
                *instructionToSplitAt = instr.get();
                return true;
            }
        }
    }
    return false;
}

class ICFGReconstructionPass: public Pass {
public:
    ICFGReconstructionPass(ICFG &icfg, BinaryPool &binaryPool,
                           const RewriterCfg &cfg,
                           const MemProtCache &memProtCache) :
            Pass(icfg, binaryPool, cfg, memProtCache, "ICFGReconstruction",
                 "Decoding and ICFG reconstruction")
    {
    }

    bool run(void)
    {
        std::unordered_map<const uint8_t *, Function *> functionMap;
        std::stack<Function *> resolveStack;

        icfg.reset();

        /*
         * Decode and install the entry function. We don't allow recursion
         * on the entry function (which will have a known prototype and
         * eventually runtime information). So don't add it to the map.
         */
        icfg.addFunction(decodeFunction(cfg.getItext()));
        resolveStack.push(icfg.getEntryFunction());
        icfg.getEntryFunction()->setInfo(cfg.getEntrySpec());

        while (!resolveStack.empty()) {
            Function *curFunction = resolveStack.top();
            CallEdgeCollector collector;

            curFunction->for_each_instruction_any(&collector);
            resolveStack.pop();
            for (auto &&edge : collector.edges) {
                const uint8_t *itext = edge.instruction->getRawTarget(
                        memProtCache);
                Function *dstFunction;

                if (!itext) {
                    /*  Unknown jump target -> call unknown code -> no edge. */
                    continue;
                }

                if (functionMap.find(itext) != functionMap.end()) {
                    /* we've seen that function already */
                    dstFunction = functionMap.find(itext)->second;
                } else {
                    /* we need a new function */
                    std::unique_ptr<Function> newFunction = decodeFunction(
                            itext);

                    dstFunction = newFunction.get();
                    /* remember that we have to process this function */
                    resolveStack.push(newFunction.get());

                    functionMap.insert(
                            std::make_pair(itext, newFunction.get()));
                    icfg.addFunction(std::move(newFunction));
                }

                /* install edges between functions and the instruction */
                edge.dst = dstFunction;
                drob_assert(edge.src);
                auto edgeptr = std::make_shared<CallEdge>(edge);
                curFunction->addOutgoingEdge(edgeptr);
                dstFunction->addIncomingEdge(edgeptr);
                edge.instruction->setCallEdge(edgeptr);
            }
        }
        return false;
    }

    std::unique_ptr<Function> decodeFunction(const uint8_t *itext)
    {
        std::unique_ptr<Function> function = std::make_unique<Function>(&icfg, itext);
        std::unordered_map<const uint8_t *, SuperBlock *> blockMap;
        std::stack<SuperBlock *> resolveStack;
        SuperBlock *dstBlock, *srcBlock;

        drob_info("Decoding function: %p (%p)", function.get(), itext);

        /* decode and add the entry block into the function */
        srcBlock = function->decodeBlock(itext, cfg);
        blockMap.insert(std::make_pair(itext, srcBlock));
        resolveStack.push(srcBlock);

        while (!resolveStack.empty()) {
            Instruction *instructionToSplitAt;
            BranchEdgeCollector collector;

            srcBlock = resolveStack.top();
            resolveStack.pop();

            /* wire up the return edge, if any (last instruction only) */
            if (!srcBlock->getInstructions().empty() &&
                srcBlock->getInstructions().back()->isRet() &&
                !srcBlock->getInstructions().back()->getReturnEdge()) {
                auto edgeptr = std::make_shared<ReturnEdge>();
                edgeptr->instruction = srcBlock->getInstructions().back().get();
                edgeptr->src = srcBlock;
                edgeptr->dst = function.get();
                function->addReturnEdge(edgeptr);
                edgeptr->instruction->setReturnEdge(edgeptr);
            }

            /* find and wire up all branch edges */
recollectEdges:
            collector.edges.clear();
            srcBlock->for_each_instruction(&collector, function.get());
            for (auto &&edge : collector.edges) {
                const uint8_t *itext = edge.instruction->getRawTarget(memProtCache);

                if (!itext) {
                    /*  Unknown branch target -> branch into unknown code -> no edge. */
                    continue;
                }

                if (blockMap.find(itext) != blockMap.end()) {
                    /* Destination already decoded */
                    dstBlock = blockMap.find(itext)->second;
                } else if (iTextCovered(blockMap, itext, &dstBlock,
                           &instructionToSplitAt)) {
                    /* Destination already decoded, but we have to split */
                    dstBlock = function->splitBlock(dstBlock, instructionToSplitAt);
                    blockMap.insert(std::make_pair(itext, dstBlock));
                    resolveStack.push(dstBlock);

                    /* We might have moved edges. The collected edges are stale */
                    goto recollectEdges;
                } else {
                    /* Destination not decoded yet */
                    dstBlock = function->decodeBlock(itext, cfg);
                    blockMap.insert(std::make_pair(itext, dstBlock));
                    resolveStack.push(dstBlock);
                }

                /* Install the branch edge */
                edge.dst = dstBlock;
                auto edgeptr = std::make_shared<BranchEdge>(edge);
                srcBlock->addOutgoingEdge(edgeptr);
                dstBlock->addIncomingEdge(edgeptr);
                edge.instruction->setBranchEdge(edgeptr);
            }
        }

        return function;
    }
};

} /* namespace drob */

#endif /* PASSES_ICFG_RECONSTRUCTION_PASS_HPP */
