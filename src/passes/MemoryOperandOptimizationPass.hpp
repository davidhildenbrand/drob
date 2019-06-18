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
#ifndef PASSES_MEMORY_OPERAND_OPTIMIZATION_PASS_HPP
#define PASSES_MEMORY_OPERAND_OPTIMIZATION_PASS_HPP

#include "../Utils.hpp"
#include "../Pass.hpp"
#include "../NodeCallback.hpp"
#include "../Instruction.hpp"

namespace drob {

/*
 * Most of this optimization is highly specialized for x86 uniform memory
 * operand definition (SIB). Most of this will most probably not
 * apply to other architectures.
 */
class MemoryOperandOptimizationPass : public Pass, public NodeCallback {
public:
    MemoryOperandOptimizationPass(ICFG &icfg, BinaryPool &binaryPool,
                                  const RewriterCfg &cfg,
                                  const MemProtCache &memProtCache) :
        Pass(icfg, binaryPool, cfg, memProtCache, "MemoryOperandOptimization",
             "Optimize memory operand addressing") {}

    bool needsStackAnalysis(void)
    {
        /* We want the actual base + index registers for indirect addressing */
        return true;
    }

    bool run(void)
    {
        icfg.for_each_instruction_any(this);
        return false;
    }

    int handleInstruction(Instruction *instruction, SuperBlock *block,
                  Function *function)
    {
        const DynamicInstructionInfo *dynInfo = instruction->getDynInfo();
        StaticOperand newOp;
        (void)block;
        (void)function;

        if (unlikely(!dynInfo))
            return 0;

        for (auto& operand : dynInfo->operands) {
            uint64_t ptrVal = 0;
            bool ptrKnown = false;

            if (unlikely(operand.isImpl))
                break;
            if (operand.type != OperandType::MemPtr)
                continue;

            /*
             * If we have a constant, we can move it to the constant pool
             * and get a better address. size==0 for LEA, there we need the
             * actual address.
             */
            if (operand.isInput && !operand.isOutput) {
                uint8_t size = static_cast<uint8_t>(operand.memAcc.size);

                if (size &&
                    ptrToInt(operand.memAcc.ptrVal, cfg, &ptrVal) &&
                    !binaryPool.isAddrContained(ptrVal) &&
                    memProtCache.isConstant(ptrVal, size)) {

                    drob_debug("Instruction %p (%p): moving memory operand to constant pool",
                               instruction, instruction->getStartAddr());
                    newOp.mem.type = MemPtrType::Direct;
                    newOp.mem.addr.val = (uint64_t)binaryPool.allocConstant((const uint8_t *) ptrVal, size);
                    newOp.mem.addr.usrPtrNr = -1;
                    goto new_operand;

                }
            }

            if (operand.memAcc.ptr.type == MemPtrType::SIB) {
                /* See if we need the complicated memory operand at all */
                if (unlikely(operand.memAcc.mode == AccessMode::None)) {
                    drob_debug("Instruction %p (%p): encoding fake direct address: 0x0",
                               instruction, instruction->getStartAddr());
                    newOp.mem.type = MemPtrType::Direct;
                    newOp.mem.addr.val = 0;
                    newOp.mem.addr.usrPtrNr = -1;
                    goto new_operand;
                }

                ptrKnown = ptrToInt(operand.memAcc.ptrVal, cfg, &ptrVal);

                /* See if we can merge base + index into disp */

                /* TODO: special case MOV, which can eat a 64bit offset. Instruction flag? */
                if (ptrKnown && isDisp32(ptrVal)) {
                    drob_debug("Instruction %p (%p): encoding direct address: %" PRIx64,
                               instruction, instruction->getStartAddr(), ptrVal);
                    newOp.mem.type = MemPtrType::Direct;
                    newOp.mem.addr.val = ptrVal;
                    if (operand.memAcc.ptrVal.isImm()) {
                        newOp.mem.addr.usrPtrNr = -1;
                    } else {
                        newOp.mem.addr.usrPtrNr = operand.memAcc.ptrVal.getNr();
                        newOp.mem.addr.usrPtrOffset = operand.memAcc.ptrVal.getPtrOffset();
                    }
                    goto new_operand;
                }

                /*
                 * See if we can merge either base or index into disp. We will
                 * start with the original operand. Disp might already contain
                 * a usr ptr from previous optimizations. We have to be careful.
                 */
                newOp = instruction->getOperand(operand.nr);

                /* Try to merge base into disp - if we have a base register */
                if (newOp.mem.sib.base != Register::None) {
                    DynamicValue tmp = addDynamicValues(operand.memAcc.ptr.sib.base,
                                       operand.memAcc.ptr.sib.disp);

                    if (ptrToInt(tmp, cfg, &ptrVal) && isDisp32(ptrVal)) {
                        drob_debug("Instruction %p (%p): dropping base register from memory operand",
                                   instruction, instruction->getStartAddr());
                        newOp.mem.sib.base = Register::None;
                        newOp.mem.sib.disp.val = ptrVal;
                        if (tmp.isImm()) {
                            newOp.mem.sib.disp.usrPtrNr = -1;
                        } else {
                            newOp.mem.sib.disp.usrPtrNr = tmp.getNr();
                            newOp.mem.sib.disp.usrPtrOffset = tmp.getPtrOffset();
                        }
                        goto new_operand;
                    }
                }

                /* Try to merge base into scale * index - if we have a index register */
                if (newOp.mem.sib.index != Register::None) {
                    DynamicValue tmp1 = multiplyDynamicValue(operand.memAcc.ptr.sib.index,
                                             operand.memAcc.ptr.sib.scale);
                    DynamicValue tmp2 = addDynamicValues(tmp1, operand.memAcc.ptr.sib.disp);

                    if (ptrToInt(tmp2, cfg, &ptrVal) && isDisp32(ptrVal)) {
                        drob_debug("Instruction %p (%p): dropping index register from memory operand",
                                   instruction, instruction->getStartAddr());
                        newOp.mem.sib.index = Register::None;
                        newOp.mem.sib.scale = 0;
                        newOp.mem.sib.disp.val = ptrVal;
                        if (tmp2.isImm()) {
                            newOp.mem.sib.disp.usrPtrNr = -1;
                        } else {
                            newOp.mem.sib.disp.usrPtrNr = tmp2.getNr();
                            newOp.mem.sib.disp.usrPtrOffset = tmp2.getPtrOffset();
                        }
                        goto new_operand;
                    }
                }
            }

            continue;
new_operand:
            instruction->setOperand(operand.nr, newOp);
            /* dynamic instruction info is dead! */
            block->invalidateStackAnalysis();
            block->invalidateLivenessAnalysis();
            /*
             * TODO: for now we only have one memory operand, but we could have more.
             * Maybe convert to "get all operands" and "set all operands" instead.
             */
            break;
        }
        return 0;
    }
};

} /* namespace drob */

#endif /* PASSES_MEMORY_OPERAND_OPTIMIZATION_PASS_HPP */
