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
#ifndef PASSES_DUMP_PASS_HPP
#define PASSES_DUMP_PASS_HPP

#include "../Pass.hpp"
#include "../NodeCallback.hpp"

namespace drob {

class DumpPass : public Pass, public NodeCallback {
public:
    DumpPass(ICFG &icfg, BinaryPool &binaryPool, const RewriterCfg &cfg,
         const MemProtCache &memProtCache) :
        Pass(icfg, binaryPool, cfg, memProtCache, "Dump", "Dump the ICFG") {}

    bool run(void)
    {
        if (!icfg.getEntryFunction()) {
            drob_dump("ICFG is empty");
            return false;
        }

        icfg.for_each_function_any(this);
        return false;
    }

    int handleInstruction(Instruction *instruction, SuperBlock *block,
                  Function *function)
    {
        (void)block;
        (void)function;
        instruction->dump();
        return 0;
    }

    int handleBlock(SuperBlock *block, Function *function)
    {
        drob_dump("%s", std::string(60,'-').c_str());
        drob_dump("Block at %p (%p)", block, block->getStartAddr());

        for (auto && edge : block->getIncomingEdges())
            drob_dump(" <- %p (%p)", edge->src, edge->src->getStartAddr());

        for (auto && edge : block->getOutgoingEdges())
            drob_dump(" -> %p (%p)", edge->dst, edge->dst->getStartAddr());

        if (block->getNext())
            drob_dump("next: %p (%p)", block->getNext(),
                      block->getNext()->getStartAddr());
        if (block->getPrev())
            drob_dump("prev: %p (%p)", block->getPrev(),
                      block->getPrev()->getStartAddr());

        if (block->getLivenessData()) {
            drob_dump("live_out: ");
            block->getLivenessData()->live_out.dump();
            drob_dump("live_in: ");
            block->getLivenessData()->live_in.dump();
        }

        drob_dump("%s", std::string(60,'-').c_str());

        block->for_each_instruction(this, function);
        return 0;
    }

    int handleFunction(Function *function)
    {
        drob_dump("Function at %p (%p)", function, function->getStartAddr());

        for (auto && edge : function->getIncomingEdges())
            drob_dump(" <- %p (%p)", edge->src, edge->src->getStartAddr());

        for (auto && edge : function->getOutgoingEdges())
            drob_dump(" -> %p (%p)", edge->dst, edge->dst->getStartAddr());

        for (auto && edge : function->getReturnEdges())
            drob_dump(" Ret from block %p (%p)", edge->src, edge->src->getStartAddr());

        function->for_each_block_any(this);
        return 0;
    }
};

} /* namespace drob */

#endif /* PASSES_DUMP_PASS_HPP */
