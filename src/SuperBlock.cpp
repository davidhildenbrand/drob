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
#include <algorithm>

#include "Utils.hpp"
#include "SuperBlock.hpp"
#include "BinaryPool.hpp"
#include "Function.hpp"
#include "Instruction.hpp"

using namespace drob;

void BranchEdge::invalidate(void) {
    drob_assert(!invalidated);
    drob_assert(instruction->getBranchEdge().get() == this);
    src->removeOutgoingEdge(this);
    dst->removeIncomingEdge(this);
    invalidated = true;
    instruction->setBranchEdge(nullptr);
    /* FIXME, object dead now -> move to caller? */
}

static void cleanupInstruction(Instruction *instruction)
{
    if (instruction->isBranch() && instruction->getBranchEdge()) {
        instruction->getBranchEdge()->invalidate();
        drob_assert(!instruction->getBranchEdge());
    } else if (instruction->isCall() && instruction->getCallEdge()) {
        instruction->getCallEdge()->invalidate();
        drob_assert(!instruction->getCallEdge());
    } else if (instruction->isRet() && instruction->getReturnEdge()) {
        instruction->getReturnEdge()->invalidate();
        drob_assert(!instruction->getReturnEdge());
    }
}

void SuperBlock::removeInstruction(Instruction *instruction)
{
    drob_info("Removing instruction: %p (%p) from %p (%p)", instruction,
               instruction->getStartAddr(), this, getStartAddr());

    /* This block is definitely dirty */
    invalidateLivenessAnalysis();
    invalidateStackAnalysis();

    cleanupInstruction(instruction);

    auto it = instrs.begin();
    for (;it != instrs.end(); it++) {
        auto && instr = *it;

        if (instr.get() == instruction) {
            /* deconstructor will handle the heavy lifting */
            instrs.erase(it);
            return;
        }
    }
    drob_assert_not_reached();
}

void SuperBlock::removeAllInstructions(void)
{
    drob_info("Removing all instructions from: %p (%p) ", this, getStartAddr());

    invalidateLivenessAnalysis();
    invalidateStackAnalysis();

    for (auto & i : instrs) {
        cleanupInstruction(i.get());
    }
    drob_assert(getOutgoingEdges().empty());

    /* invalidate and remove all incoming edges */
    std::vector<std::shared_ptr<BranchEdge>> copiedEdges = incomingEdges;
    for (auto & edge : copiedEdges) {
        edge->invalidate();
    }
    drob_assert(getIncomingEdges().empty());
    instrs.clear();
}
