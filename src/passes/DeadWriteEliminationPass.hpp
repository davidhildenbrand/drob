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
#ifndef PASSES_DEAD_WRITE_ELIMINATION_PASS_HPP
#define PASSES_DEAD_WRITE_ELIMINATION_PASS_HPP

#include <vector>
#include "../Utils.hpp"
#include "../Pass.hpp"
#include "../NodeCallback.hpp"
#include "../Instruction.hpp"

namespace drob {

/*
 * We assume that liveness analysis will set liveness data for all instructions
 * that cannot be removed and clear it for all instructions that can be removed.
 */
class DeadWriteEliminationPass: public Pass, public NodeCallback {
public:
    DeadWriteEliminationPass(ICFG &icfg, BinaryPool &binaryPool,
                         const RewriterCfg &cfg,
                         const MemProtCache &memProtCache) :
            Pass(icfg, binaryPool, cfg, memProtCache, "DeadWriteRemoval",
                 "Remove instructions that perform dead writes to registers")
    {
    }

    bool run(void)
    {
        /* TODO: analyze all functions. */
        if (!icfg.getEntryFunction())
            return false;
        icfg.getEntryFunction()->for_each_instruction_any(this);

        /* Remove all instruction */
        for (auto &i : instructionsToDelete) {
            i.second->removeInstruction(i.first);
            icfg.livenessAnalysisValid  = false;
            icfg.getEntryFunction()->livenessAnalysisValid  = false;
            i.second->livenessAnalysisValid = false;
        }
        instructionsToDelete.resize(0);
        return false;
    }

    bool needsLivenessAnalysis(void)
    {
        /* We need a valid liveness analysis to identify instructions to remove */
        return true;
    }

    int handleInstruction(Instruction *instruction, SuperBlock *block,
                          Function *function)
    {
        (void)function;
        /*
         * FIXME: liveness analysis mostly works with static instruction
         * information. This might hinder some instructions from getting removed.
         */
        if (!instruction->getLivenessData()) {
            instructionsToDelete.push_back(std::make_pair(instruction, block));
        }
        return 0;
    }
    std::vector<std::pair<Instruction *, SuperBlock *>> instructionsToDelete;
};

} /* namespace drob */

#endif /* PASSES_DEAD_WRITE_ELIMINATION_PASS_HPP */
