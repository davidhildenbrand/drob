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
#include "Utils.hpp"
#include "Function.hpp"

namespace drob {

void CallEdge::invalidate(void)
{
    drob_assert(!invalidated);
    drob_assert(instruction->getCallEdge().get() == this);
    src->removeOutgoingEdge(this);
    dst->removeIncomingEdge(this);
    invalidated = true;
    instruction->setCallEdge(nullptr);
    /* FIXME, object dead now -> move to caller? */
}

void ReturnEdge::invalidate(void)
{
    drob_assert(!invalidated);
    drob_assert(instruction->getReturnEdge().get() == this);
    /*
     * For now, a block can only have on return instruction and it is the
     * last one (no conditional ret instructions yet). Therefore, no need to
     * store the edge at the source block, only at the instruction.
     */
    dst->removeReturnEdge(this);
    invalidated = true;
    instruction->setReturnEdge(nullptr);
    /* FIXME, object dead now -> move to caller? */
}

static void cleanupBlock(SuperBlock *block)
{
    /* remove all instructions along with all edges */
    block->removeAllInstructions();

    /* reconfigure prev/next */
    if (block->getPrev()) {
        block->getPrev()->setNext(block->getNext());
    }
    if (block->getNext()) {
        block->getNext()->setPrev(block->getPrev());
    }
}

void Function::removeBlock(SuperBlock *block)
{
    drob_info("Removing block %p (%p) from %p (%p)", block,
               block->getStartAddr(), this, getStartAddr());

    if (entryBlock == block)
        entryBlock = nullptr;

    /* Edges will be handled when removing the instructions */
    if (block->next) {
        block->next->invalidateStackAnalysis();
    }
    if (block->prev) {
        block->prev->invalidateLivenessAnalysis();
    }

    cleanupBlock(block);

    auto it = blocks.begin();
    for (;it != blocks.end(); it++) {
        if ((*it).get() == block) {
            /* deconstructor will handle the heavy lifting */
            blocks.erase(it);
            return;
        }
    }
    drob_assert_not_reached();
}

void Function::removeAllBlocks(void)
{
    drob_info("Removing all blocks from %p (%p)", this, getStartAddr());

    entryBlock = nullptr;

    invalidateLivenessAnalysis();
    invalidateStackAnalysis();

    for (auto & block : blocks) {
        cleanupBlock(block.get());
    }
    blocks.clear();
}

void Function::mergeBlockIntoNext(SuperBlock *block)
{
    drob_assert(block->next);

    /*
     * Don't allow merging into a block with incoming edges. Make analysis
     * properly complicated and is usually wrong either way.
     */
    drob_assert(block->next->getIncomingEdges().empty());

    drob_info("Merging block %p (%p) into %p (%p)",
              block, block->getStartAddr(), block->next,
              block->next->getStartAddr());

    if (entryBlock == block) {
        entryBlock = block->next;
    }

    /* Fixup prev/next pointers */
    if (block->prev) {
        block->prev->next = block->next;
    }
    block->next->prev = block->prev;

    /* Fixup the edges */
    for (auto && edge : block->incomingEdges) {
        drob_assert(!edge->invalidated);
        drob_assert(edge->instruction->getBranchEdge() == edge);
        drob_assert(edge->dst == block)
        edge->dst = block->next;
    }
    for (auto && edge : block->outgoingEdges) {
        drob_assert(!edge->invalidated);
        drob_assert(edge->instruction->getBranchEdge() == edge);
        drob_assert(edge->src == block)
        edge->src = block->next;
    }

    /* Move all edges, properly prepend outgoing edges to keep the order */
    block->next->incomingEdges.insert(block->next->incomingEdges.end(),
                                      std::make_move_iterator(block->incomingEdges.begin()),
                                      std::make_move_iterator(block->incomingEdges.end()));
    block->incomingEdges.clear();
    block->next->outgoingEdges.insert(block->next->outgoingEdges.begin(),
                                      std::make_move_iterator(block->outgoingEdges.begin()),
                                      std::make_move_iterator(block->outgoingEdges.end()));
    block->outgoingEdges.clear();

    /* Fixup return edges we don't store yet in the block */
    for (auto & instr : block->instrs) {
        if (instr->isRet() && instr->getReturnEdge()) {
            auto & edge = instr->getReturnEdge();
            drob_assert(edge->src == block);
            edge->src = block->next;
        }
    }

    /* move all instructions to the beginning of the next block */
    block->next->instrs.splice(block->next->instrs.begin(), block->instrs);
    drob_assert(block->instrs.empty());

    /*
     * live_out of the next block is still valid. live_in of this block is
     * valid (no incoming edges on next).
     */
    if (block->getLivenessData() && block->next->getLivenessData()) {
        block->next->getLivenessData()->live_in = block->getLivenessData()->live_in;
    }
    block->next->livenessAnalysisValid &= block->livenessAnalysisValid;

    /*
     * The ProgramState of the next block is incorrect, we have to move
     * the state.
     */
    if (block->getEntryState()) {
        /* TODO: proper move */
        block->next->setEntryState(std::make_unique<ProgramState>(*block->getEntryState()));
    }
    block->next->stackAnalysisValid &= block->stackAnalysisValid;

    /* Delete the block */
    removeBlock(block);
}

SuperBlock* Function::splitBlock(SuperBlock *block,
                                 const std::list<std::unique_ptr<Instruction>>::iterator& it)
{
    std::unique_ptr<SuperBlock> newBlock = std::make_unique<SuperBlock>(this);
    SuperBlock *ret = newBlock.get();

    /* wire up fallthrough chain */
    if (block->next) {
        block->next->prev = newBlock.get();
        newBlock->next = block->next;
    }
    newBlock->prev = block;
    block->next = newBlock.get();

    /* we dropped NOPs, could be there is nothing left */
    if (it != block->instrs.end()) {
        /* move all instructions */
        newBlock->instrs.splice(newBlock->instrs.end(), block->instrs, it,
                                block->instrs.end());
    }

    /* TODO can we move the edges directly instead? */
    for (auto && instr : newBlock->instrs) {
        if (instr->isBranch() && instr->getBranchEdge()) {
            auto& edge = instr->getBranchEdge();

            /* move the edge */
            drob_assert(!edge->invalidated);
            drob_assert(edge->instruction->getBranchEdge() == edge);

            /*
             * We only care about the source of the edge. The destination
             * will always be the old block. The will maintain the order of
             * outgoing edges.
             */
            if (edge->src == block) {
                edge->src = newBlock.get();
                newBlock->outgoingEdges.push_back(edge);
                block->removeOutgoingEdge(edge.get());
            }
        } else if (instr->isRet() && instr->getReturnEdge()) {
            auto& edge = instr->getReturnEdge();

            /* reconfigure the edge */
            drob_assert(!edge->invalidated);
            drob_assert(edge->instruction->getReturnEdge() == edge);

            /* we don't store return edges per block yet */
            if (edge->src == block) {
                edge->src = newBlock.get();
            }
        }
    }

    /* The old block has no valid live_out */
    block->invalidateStackAnalysis();

    /* The new block as no program state and no livenes information. */
    newBlock->invalidateLivenessAnalysis();
    newBlock->invalidateStackAnalysis();

    addBlock(std::move(newBlock));
    return ret;
}

SuperBlock* Function::splitBlock(SuperBlock *block, Instruction *instruction)
{
    drob_info("Splitting block: %p (%p) at instruction %p (%p)",
              block, block->getStartAddr(), instruction,
              instruction->getStartAddr());

    /* select all instructions to move */
    std::list<std::unique_ptr<Instruction>>::iterator it;
    for (it =  block->instrs.begin(); it !=  block->instrs.end(); ++it){
        if (it->get() == instruction) {
            break;
        }
    }
    return splitBlock(block, it);
}

SuperBlock* Function::splitBlockAfter(SuperBlock *block, Instruction *instruction)
{
    drob_info("Splitting block: %p (%p) after instruction %p (%p)",
              block, block->getStartAddr(), instruction,
              instruction->getStartAddr());

    /* select all instructions to move */
    std::list<std::unique_ptr<Instruction>>::iterator it;
    for (it =  block->instrs.begin(); it !=  block->instrs.end(); ++it){
        if (it->get() == instruction) {
            it++;
            break;
        }
    }
    return splitBlock(block, it);
}

SuperBlock* Function::copyBlock(SuperBlock *block)
{
    std::unique_ptr<SuperBlock> newBlock = std::make_unique<SuperBlock>(this);
    SuperBlock *blockRet = newBlock.get();

    /*
     * Copy instruction one by one, copying and fixing up outgoing edges.
     */
    for (auto & instr : block->getInstructions()) {
        std::unique_ptr<Instruction> newInstr = std::make_unique<Instruction>(*instr);

        if (instr->isBranch() && instr->getBranchEdge()) {
            auto newEdge = std::make_shared<BranchEdge>();

            newEdge->src = newBlock.get();
            newEdge->dst = instr->getBranchEdge()->dst;
            /* loop */
            if (newEdge->dst == block) {
                newEdge->dst = newBlock.get();
            }
            newEdge->instruction = newInstr.get();

            newInstr->setBranchEdge(newEdge);
            newEdge->src->addOutgoingEdge(newEdge);
            newEdge->dst->addIncomingEdge(newEdge);

        } else if (instr->isCall() && instr->getCallEdge()) {
            auto newEdge = std::make_shared<CallEdge>();

            newEdge->src = instr->getCallEdge()->src;
            newEdge->dst = instr->getCallEdge()->dst;
            newEdge->instruction = newInstr.get();

            newInstr->setCallEdge(newEdge);
            newEdge->src->addOutgoingEdge(newEdge);
            newEdge->dst->addIncomingEdge(newEdge);

        } else if (instr->isRet() && instr->getReturnEdge()) {
            auto newEdge = std::make_shared<ReturnEdge>();

            newEdge->src = newBlock.get();
            newEdge->dst = instr->getReturnEdge()->dst;
            newEdge->instruction = newInstr.get();

            newInstr->setReturnEdge(newEdge);
            newEdge->dst->addReturnEdge(newEdge);
        }
        newBlock->appendInstruction(newInstr);
    }

    /* FIXME: Invalidate stack + liveness analysis */

    addBlock(std::move(newBlock));
    return blockRet;
}

SuperBlock* Function::decodeBlock(const uint8_t *itext, const RewriterCfg& cfg)
{
    std::unique_ptr<SuperBlock> newBlock = std::make_unique<SuperBlock>(this);
    std::list<std::unique_ptr<Instruction>> instrs;
    SuperBlock *blockRet = newBlock.get();
    DecodeRet ret;

    drob_info("Decoding block %p (%p)", newBlock.get(), itext);

    while(true) {
        const uint64_t addr = (uint64_t)itext;
        uint16_t max_ilen = DIV_ROUND_UP(addr, ARCH_PAGE_SIZE) - addr;

        if (!max_ilen || max_ilen > ARCH_MAX_ILEN) {
            max_ilen = ARCH_MAX_ILEN;
        }
        drob_assert(instrs.empty());

        /* first try to decode without crossing page boundaries */
        ret = arch_decode_one(&itext, max_ilen, instrs, cfg);
        if (ret == DecodeRet::BrokenInstr && max_ilen < ARCH_MAX_ILEN) {
            /* next read over the page boundary */
            ret = arch_decode_one(&itext, ARCH_PAGE_SIZE, instrs, cfg);
        }
        if (unlikely(ret == DecodeRet::UnhandledInstr)) {
            drob_throw("Unhandled instruction");
        } else if (unlikely(ret == DecodeRet::UnsupportedInstr)) {
            drob_throw("Unsupported instruction");
        } else if (unlikely(ret == DecodeRet::BrokenInstr)) {
            drob_throw("Broken instruction stream");
        }

        if (ret != DecodeRet::NOP) {
            drob_assert(!instrs.empty());
            newBlock->addInstructions(instrs);
        }

        if (ret == DecodeRet::EOB) {
            /* end of block, e.g. branch, jmp, call, ret ... */
            break;
        }
        drob_assert(ret == DecodeRet::Ok || ret == DecodeRet::NOP);
    }

    invalidateLivenessAnalysis();
    invalidateLivenessAnalysis();

    addBlock(std::move(newBlock));
    return blockRet;
}

}; /* namespace drob */
