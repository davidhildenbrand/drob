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
#ifndef PASSES_LIVENESS_ANALYSIS_PASS_HPP
#define PASSES_LIVENESS_ANALYSIS_PASS_HPP

#include "../Pass.hpp"

namespace drob {

/*
 * TODO: Delta liveness analysis is imprecise as soon as we have loops.
 * Once a register is "trapped" as "live" inside a loop, it will never
 * get removed.
 *
 * To handle this case, all blocks part of the loop have to be fully
 * marked as invalid AND the liveness data has to be cleared.
 *
 * But things get more complicated: If we have previous liveness data and
 * we mark any block in our algorithm as invalid, we could potentially
 * have a loop to invalidate. This would have to be checked whenever we
 * modify the liveness data of a block, getting horribly complicated and
 * most probably slower as if we simply throw away anything for now and
 * start anew.
 *
 * So for now, don't do a delta analysis, always start fresh, as liveness
 * analysis data is pretty important for our optimizations and we want it to
 * be precise.
 *
 * Example:
 *
 * rax = 0;
 * while (some_cond) {
 *      do_something_without_rax();
 * }
 * rbx = rax;
 *
 * -> rax is trapped as alive in the loop, even though if the "rbx = rax"
 * part has been removed.
 * -> rax = 0 can never get removed.
 *
 * Same applies to:
 *
 * rax = 0;
 * while (some_cond) {
 *      rbx = rax;
 *      do_something_without_rax();
 * }
 *
 * Similar things apply to delta stack analysis.
 */

class ClearLivenessData : public NodeCallback {
    int handleBlock(SuperBlock * block, Function *function)
    {
        /*
         * We won't remove liveness analysis attached to instructions. This
         * might allow to reuse some calculated masks.
         */
        (void)function;
        block->setLivenessData(nullptr);
        block->livenessAnalysisValid = false;
        return 0;
    }
};

/*
 * The liveness analysis will currently not perform a real delta analysis,
 * instead all blocks are reanalyzed.
 *
 * Values will then fully be propagated backwards through the CFG, performing
 * updates where really necessary.
 *
 * We don't need a valid stack analysis (we will usually run before stack
 * analysis to avoid propagating actually dead values trough registers, to
 * simplify/speed up delta stack analysis), but will make of the available
 * data to eventually make a more precise analysis once we have stack analysis
 * data.
 */
class LivenessAnalysisPass : public Pass, NodeCallback {
public:
    LivenessAnalysisPass(ICFG &icfg, BinaryPool &binaryPool,
               const RewriterCfg &cfg, const MemProtCache &memProtCache) :
        Pass(icfg, binaryPool, cfg, memProtCache, "LivenessAnalysis",
             "Perform a (register) liveness analysis") {}

    /*
     * Live/Dead registers needed for
     * - Encoding if new registers are needed (e.g. to load an address)
     * - Wanting to add new instructions (e.g. vectorize code)
     * - Detect if instructions with dead writes can be removed
     * - Register reallocation
     * - Undo stack spilling
     *
     * "Flows backwards"
     * - Collect ret instructions per function.
     * - Return value register + preserved registers are alive "after return"
     * -- difficult for !entry functions! -> skip for now
     * - Walk backwards, computing registers that are alive ("instruction needs
     *   register")
     * -- Predicate registers
     * -- Read registers if instruction may execute ("may read")
     * - Register is Dead once
     * -- Written and instruction will execute
     * - Merging of masks necessary
     * -- Easy, OR of live registers
     * - Remember live registers per instruction
     * -- live_in and live_out
     * - Remember live registers per edge
     */
    bool run(void)
    {
        if (icfg.livenessAnalysisValid) {
            return false;
        }

        /* For now we only handle the entry function */
        if (!icfg.getEntryFunction())
            return false;
        handleFunction(icfg.getEntryFunction());

        icfg.livenessAnalysisValid = true;
        return false;
    }

    /*
     * Compute and store liveness information. Return a pointer to the
     * live_in data.
     */
    const SubRegisterMask *processInstruction(Instruction *instruction,
                                              const SubRegisterMask *live)
    {
        LivenessData tmpLivenessData;
        const SubRegisterMask *readRegs;
        const SubRegisterMask *writtenRegs;
        const SubRegisterMask *condWrittenRegs;
        const SubRegisterMask *predicateRegs;
        bool mayWriteMem;
        TriState willExecute;

        drob_info("Analyzing instruction %p (%p)", instruction,
                  instruction->getStartAddr());

        /*
         * If we have dynamic instruction info, make use of it, otherwise use
         * the less precise static information.
         */
        if (instruction->getDynInfo()) {
            const DynamicInstructionInfo *dynInfo = instruction->getDynInfo();

            readRegs = &dynInfo->readRegs;
            writtenRegs = &dynInfo->writtenRegs;
            condWrittenRegs = &dynInfo->condWrittenRegs;
            predicateRegs = &dynInfo->predicateRegs;
            willExecute = dynInfo->willExecute;
            mayWriteMem = dynInfo->mayWriteMem;
        } else {
            const InstructionInfo &info = instruction->getInfo();

            readRegs = &info.readRegs;
            writtenRegs = &info.writtenRegs;
            condWrittenRegs = &info.condWrittenRegs;
            predicateRegs = &info.predicateRegs;
            if (instruction->getPredicate()) {
                willExecute = TriState::Unknown;
            } else {
                willExecute = TriState::True;
            }
            mayWriteMem = info.mayWriteMem;
        }

        /*
         * Any instruction that is not a control flow instruction can be
         * removed if
         * - It will not write memory
         * - All register it writes are never needed (dead)
         * Indicate this by not attaching liveness information. Also, as we assume
         * these instructions are essentially dead and can be removed, don't
         * consider registers that are read by such an instruction as alive.
         */
        if (!instruction->isBranch() && !instruction->isCall() &&
            !instruction->isRet() && !mayWriteMem) {
            SubRegisterMask tmp = *live;

            tmp -= *writtenRegs;
            tmp -= *condWrittenRegs;
            if (tmp == *live) {
                drob_debug("Instruction dead");
                /* Clear analysis data, if any. Marker that this instruction can go. */
                instruction->setLivenessData(nullptr);
                return live;
            }
        }

        /*
         * Calculate the actual effects of this instruction. Special handling
         * required for predicated instructions and control flow instructions.
         */
        tmpLivenessData.live_out = *live;
        tmpLivenessData.live_in = *live;

        switch (willExecute) {
        case TriState::True:
            /* definitely written registers are canceled out */
            tmpLivenessData.live_in -= *writtenRegs;
            /* fall through */
        case TriState::Unknown:
            /* we need all registers we read (except predicates) */
            tmpLivenessData.live_in += *readRegs;
            if (instruction->isRet()) {
                tmpLivenessData.live_in = live_ret;
            }
            /* TODO: Calls can do anything for now */
            if (instruction->isCall()) {
                tmpLivenessData.live_in.fill();
            }
            /* Handle branch edges and branches to unknown code */
            if (instruction->isBranch()) {
                auto & edge = instruction->getBranchEdge();

                if (edge) {
                    /*
                     * If we already have a state for an edge, merge that. If not,
                     * we will have to revisit later once we have edge data. We will
                     * get reenqueued.
                     */
                    if (edge->dst->getLivenessData()) {
                        tmpLivenessData.live_in += edge->dst->getLivenessData()->live_in;
                    }
                } else {
                    /* branching into unknown code -> anything possible */
                    tmpLivenessData.live_in.fill();
                }
            }
            break;
        case TriState::False:
        default:
            /* instruction not executed, no registers read/written */
            break;
        }
        /* predicate registers are always read */
        tmpLivenessData.live_in += *predicateRegs;

        drob_debug("Setting liveness information");
        if (instruction->getLivenessData()) {
            *instruction->getLivenessData() = tmpLivenessData;
        } else {
            instruction->setLivenessData(std::make_unique<LivenessData>(tmpLivenessData));
        }
        return &instruction->getLivenessData()->live_in;
    }

    /*
     * Process a block and all it's predecessors backwards, starting at the
     * given instruction.
     */
    void processBlock(SuperBlock *block)
    {
        auto & instrs = block->getInstructions();
        const SubRegisterMask *live;
        bool entryLiveRegsChanged = false;

        /* We should not be enqueued twice */
        drob_assert(!block->livenessAnalysisValid);

        /*
         * If we have a next block and it has no liveness data yet, enqueue it.
         * It will properly reenqueue this block, once processed.
         */
        if (block->getNext() && !block->getNext()->getLivenessData()) {
            drob_assert(!block->getNext()->livenessAnalysisValid);
            if (!block->getNext()->queued) {
                block->getNext()->queued = true;
                blocksToProcess.push(block->getNext());
            }
            return;
        }

        drob_info("Analyzing block %p (%p)", block, block->getStartAddr());

        /* Attach empty liveness information, if none is attached. */
        if (unlikely(!block->getLivenessData())) {
            LivenessData tmp = {};

            block->setLivenessData(std::make_unique<LivenessData>(tmp));
        }

        /*
         * Set live_out of this block. If we have a next block, copy its
         * live_in. Otherwise the last instruction in this block
         * has to be a control flow instruction, which will properly handle
         * live_out, depending on the target. Start with empty live_out.
         *
         * Don't mess with live_in, this has to stay untouched due to self-branches.
         */
        if (block->getNext()) {
            block->getLivenessData()->live_out = block->getNext()->getLivenessData()->live_in;
        } else {
            block->getLivenessData()->live_out.zero();
        }

        /* Propagate liveness information backwards through all instructions. */
        live = &block->getLivenessData()->live_out;
        for (auto it = instrs.rbegin(); it != instrs.rend(); ++it) {
            drob_assert(live);
            live = processInstruction(it->get(), live);
        }

        /*
         * Remember the entry state, so edges/prev can find it. Also compute if
         * there was an actual change, because then we have to propagate it,
         * too.
         */
        if (block->getLivenessData()->live_in != *live) {
            block->getLivenessData()->live_in = *live;
            entryLiveRegsChanged = true;
        }

        /*
         * Set this block to valid before eventually finding it in the incoming
         * edges list again, having to mark the analysis invalid (self-branches).
         */
        block->livenessAnalysisValid = true;

        /*
         * Propagate the values backwards the incoming branch edges. Always
         * reevaluate these blocks completely. Make sure to enqueue all
         * blocks with invalid liveness analysis that are not enqueued,
         * otherwise we might not analyze all blocks.
         */
        for (auto & edge : block->getIncomingEdges()) {
            if (entryLiveRegsChanged) {
                edge->src->livenessAnalysisValid = false;
            }

            if (!edge->src->livenessAnalysisValid && !edge->src->queued) {
                edge->src->queued = true;
                blocksToProcess.push(edge->src);
            }
        }
        if (block->getPrev()) {
            SuperBlock *prev = block->getPrev();

            if (entryLiveRegsChanged) {
                prev->livenessAnalysisValid = false;
            }

            if (!prev->livenessAnalysisValid && !prev->queued) {
                prev->queued = true;
                blocksToProcess.push(prev);
            }
        }
    }

    int handleFunction(Function *function)
    {
        const FunctionSpecification *spec = function->getInfo();

        if (function->livenessAnalysisValid) {
            return 0;
        }
        drob_info("Analyzing function %p (%p)", function,
                  function->getStartAddr());

        if (!spec) {
            drob_debug("Function has no specification");
            return 0;
        }

        /* Construct the registers that are alive when returning. */
        live_ret = spec->reg.out;
        live_ret += spec->reg.preserved;

        /* Throw away old analysis data. */
        ClearLivenessData clearLivenessData;
        function->for_each_block_any(&clearLivenessData);

        /* Start with all superblocks that have return edges. */
        drob_assert(!function->getReturnEdges().empty());
        for (auto & edge : function->getReturnEdges()) {
            SuperBlock *block = edge->src;

            block->queued = true;
            blocksToProcess.push(block);
        }

        /* Process all superblocks until there are no changes anymore. */
        while (!blocksToProcess.empty()) {
            auto & block = blocksToProcess.front();
            blocksToProcess.pop();
            block->queued = false;

            processBlock(block);
        }

        function->livenessAnalysisValid = true;
        return 0;
    }
private:
    /*
     * Queue of edges we'll have to process.
     */
    std::queue<SuperBlock *> blocksToProcess;
    /*
     * Registers alive after returning from the function. (preserved registers
     * and registers used to return values)
     */
    SubRegisterMask live_ret {};
};

} /* namespace drob */

#endif /* PASSES_LIVENESS_ANALYSIS_PASS_HPP */
