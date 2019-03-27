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
#ifndef PASSES_CODE_GENERATION_PASS_HPP
#define PASSES_CODE_GENERATION_PASS_HPP

#include "../Pass.hpp"

namespace drob {

class CodeGenerationPass : public Pass, public NodeCallback {
public:
    CodeGenerationPass(ICFG &icfg, BinaryPool &binaryPool,
               const RewriterCfg &cfg, const MemProtCache &memProtCache) :
        Pass(icfg, binaryPool, cfg, memProtCache, "CodeGeneration", "Generate code") {}

    int handleInstruction(Instruction *instruction, SuperBlock *block,
                  Function *function)
    {
        (void)block;
        (void)function;
        if (instruction->isBranch()) {
            auto &edge = instruction->getBranchEdge();

            if (edge) {
                BranchLocation branch = arch_prepare_branch(*instruction, binaryPool);

                /* see if we already know the branch target */
                if (blockMap.find(edge->dst) != blockMap.end()) {
                    arch_fixup_branch(branch, blockMap.find(edge->dst)->second,
                                      write);
                } else {
                    branches.push_back(branch);
                }
                return 0;
            }
        } else if (instruction->isCall()) {
           auto &edge = instruction->getCallEdge();

            if (edge) {
                CallLocation call = arch_prepare_call(*instruction, binaryPool);

                calls.push_back(call);
                return 0;
            }
        }

        drob_assert(!instruction->isRet() || instruction->getReturnEdge());

        instruction->generateCode(binaryPool,  write);
        return 0;
    }

    int handleBlock(SuperBlock *block, Function *function)
    {
        drob_debug("Block at [%p - %p]", block->getStartAddr(),
               block->getEndAddr());

        blockMap.insert(std::make_pair(block, binaryPool.newBlock(write)));
        drob_assert(!block->getInstructions().empty() && "Empty block");
        block->for_each_instruction(this, function);
        return 0;
    }

    int handleFunction(Function *function)
    {
        drob_debug("Function at [%p]", function->getStartAddr());
        drob_assert(function->getEntryBlock() && "Empty function");

        /* Walk all blocks starting with the entry */
        functionMap.insert(std::make_pair(function, binaryPool.newBlock(write)));
        function->for_each_block_dfs(this);

        /* fixup all branches */
        for (auto && branch : branches) {
            auto &edge = branch.instr->getBranchEdge();

            drob_assert(blockMap.find(edge->dst) != blockMap.end());
            const uint8_t *itext = blockMap.find(edge->dst)->second;
            arch_fixup_branch(branch, itext, write);
        }
        branches.clear();

        return 0;
    }

    bool run(void)
    {
        /* Reset our code buffer */
        binaryPool.resetCodePool();

        /* Walk all functions starting with the entry */
        icfg.for_each_function_dfs(this);

        /* Fixup all calls */
        for (auto && call : calls) {
            auto &edge = call.instr->getCallEdge();

            drob_assert(functionMap.find(edge->dst) != functionMap.end());
            const uint8_t *itext = functionMap.find(edge->dst)->second;
            arch_fixup_call(call, itext, write);
        }
        calls.clear();

        functionMap.clear();
        blockMap.clear();

        if (!write) {
            /* write on the next run */
            write = true;
            return true;
        }
        return false;
    }

    void reset(void)
    {
        write = false;
        functionMap.clear();
        blockMap.clear();
    }

private:
    /*
     * We'll do two runs, one run where we don't write code but only
     * fake-allocate memory in the code pool, so we can detect if
     * using short branches would work to jump to a pool. In the second
     * run, we'll then generate compressed code based on the previously
     * calcualted information.
     */
    bool write{false};
    std::unordered_map<const Function*, const uint8_t *> functionMap;
    std::unordered_map<const SuperBlock *, const uint8_t *> blockMap;
    std::vector<BranchLocation> branches;
    std::vector<CallLocation> calls;
};

} /* namespace drob */

#endif /* PASSES_CODE_GENERATION_PASS_HPP */
