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
#ifndef PASSES_INSTRUCTION_SPECIALIZATION_PASS_HPP
#define PASSES_INSTRUCTION_SPECIALIZATION_PASS_HPP

#include "../Pass.hpp"

namespace drob {

/*
 * We can try to specialize a function once we have more known input operands
 * (immediates) than originally encoded. E.g. for ADC it will already
 * help to know the CF only to eventually convert it into an ADD.
 */
class InstructionSpecializationPass : public Pass, public NodeCallback {
public:
    InstructionSpecializationPass(ICFG &icfg, BinaryPool &binaryPool,
               const RewriterCfg &cfg, const MemProtCache &memProtCache) :
        Pass(icfg, binaryPool, cfg, memProtCache, "InstructionSpecialization",
             "Specialize instruction to known parameters") {}

    bool needsStackAnalysis(void)
    {
        /* We need to known runtime operands */
        return true;
    }

    bool needsLivenessAnalysis(void)
    {
        /*
         * We need to known if e.g. eflags written by an instruction
         * are relevant or not. This might allow to convert some instructions
         * to pure MOVs - if eflags are not necessary.
         */
        return true;
    }

    int handleInstruction(Instruction * instruction, SuperBlock *block,
                          Function *function)
    {
        const DynamicInstructionInfo *dynInfo = instruction->getDynInfo();
        const LivenessData *livenessData = instruction->getLivenessData();
        const OpcodeInfo *opi = instruction->getOpcodeInfo();
        SpecRet ret;

        (void)function;

        if (unlikely(!dynInfo || !livenessData || !opi || !opi->specialize))
            return 0;

        drob_info("Trying to specialize instruction %p (%p)",
                  instruction, instruction->getStartAddr());

        /*
         * Copy the opcode and the operands so we can modify them.
         */
        Opcode opcode = instruction->getOpcode();
        ExplicitStaticOperands rawOperands = instruction->getOperands();

        ret = opi->specialize(opcode, rawOperands, *dynInfo, *livenessData, cfg,
                              binaryPool);
        if (ret == SpecRet::Change) {
            drob_info("-> Changing instruction");

            /* TODO rework this whole setting */
            instruction->setOpcode(opcode);
            for (int i = 0; i < instruction->getNumOperands(); i++) {
                instruction->setOperand(i, rawOperands.op[i]);
            }

            block->invalidateLivenessAnalysis();
            block->invalidateStackAnalysis();
        } else if (ret == SpecRet::Delete) {
            drob_info("-> Deleting instruction");
            instrToDelete.emplace_back(std::make_pair(instruction, block));
        }

        return 0;
    }

    int handleFunction(Function *function)
    {
        /* We might only have data for the entry function */
        if (unlikely(!function->stackAnalysisValid ||
                     !function->livenessAnalysisValid))
            return 0;
        function->for_each_instruction_any(this);
        return 0;
    }

    bool run(void)
    {
        icfg.for_each_function_any(this);

        for (auto & pair : instrToDelete) {
            pair.second->removeInstruction(pair.first);
        }
        instrToDelete.clear();
        return false;
    }

    std::vector<std::pair<Instruction *, SuperBlock *>> instrToDelete;
};

} /* namespace drob */

#endif /* PASSES_INSTRUCTION_SPECIALIZATION_PASS_HPP */
