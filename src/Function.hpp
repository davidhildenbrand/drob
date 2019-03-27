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
#ifndef FUNCTION_HPP
#define FUNCTION_HPP

#include <vector>
#include <queue>
#include <stack>
#include <algorithm>

#include "Utils.hpp"
#include "Node.hpp"
#include "SuperBlock.hpp"
#include "NodeCallback.hpp"
#include "RegisterInfo.hpp"
#include "ProgramState.hpp"
#include "RewriterCfg.hpp"

namespace drob {

class Instruction;
class ICFG;

/*
 * Edge between functions. The responsible call instruction is
 * also contained.
 */
typedef struct CallEdge {
    Function *dst;
    Function *src;
    Instruction *instruction;
    bool invalidated{false};

    /* FIXME: store the source block */

    void invalidate(void);
} FunctionEdge;

/*
 * Edge between an instruction/block and its containing function.
 */
typedef struct ReturnEdge {
    Function *dst;
    SuperBlock *src;
    Instruction *instruction;
    bool invalidated{false};

    void invalidate(void);
} ReturnEdge;

class Function : public Node {
public:
    Function(ICFG *icfg, const uint8_t *itext) :
        Node((Node *)icfg), icfg(icfg), itext(itext)
    {
    }

    /*
     * Set the function info, e.g. based on user function specification
     * or as detected while analyzing a function. A local copy will
     * be created.
     */
    void setInfo(const FunctionSpecification &info)
    {
        this->info.reset(new FunctionSpecification(info));
    }

    /*
     * Get the function info. Is there is none, we have to assume the
     * function can do anything (read/write all registers, stack
     * unpredictable).
     */
    const FunctionSpecification* getInfo(void) const
    {
        return info.get();
    }

    /*
     * Get the entry block into this function. All other blocks can
     * be reached from this block.
     */
    SuperBlock *getEntryBlock() const
    {
        return entryBlock;
    }

    /*
     * Set the entry block into the function. It already has to be
     * added to the function.
     */
    void setEntryBlock(SuperBlock *block)
    {
        entryBlock = block;
    }

    /*
     * Get the original start address of the code this function is
     * based on.
     */
    const uint8_t *getStartAddr(void)
    {
        return itext;
    }

    /*
     * Go over all blocks in any order. (including unconnected nodes)
     */
    int for_each_block_any(NodeCallback *cb)
    {
        int ret;

        for (auto && block : blocks) {
            ret = cb->handleBlock(block.get(), this);
            if (ret)
                return ret;
        }
        return 0;
    }

    /*
     * Mark all blocks as visited or not visited.
     */
    void mark_all_blocks(bool visited)
    {
        for (auto && b : blocks) {
            b->visited = visited;
        }
    }

    /*
     * Go over all blocks in depth-first search order.
     */
    int for_each_block_dfs(NodeCallback *cb)
    {
        std::stack<SuperBlock *> blocks;
        int ret;

        if (!entryBlock)
            return 0;
        if (entryBlock->visited)
            mark_all_blocks(false);
        blocks.push(entryBlock);

        while (!blocks.empty()) {
            SuperBlock *b = blocks.top();

            blocks.pop();
            if (b->visited)
                continue;

            /*
             * We treat fallthrough blocks as if it would be
             * one big block. (well it actually is, we just had
             * to split it)
             */
            do {
                if (b->visited) {
                    b = b->getNext();
                    continue;
                }
                b->visited = true;
                ret = cb->handleBlock(b, this);
                if (ret)
                    return ret;

                /*
                 * Add in reverse order to the stack so the
                 * first branch will be processed next.
                 */
                auto edges = b->getOutgoingEdges();
                for (auto it = edges.rbegin(); it != edges.rend(); ++it) {
                    const std::shared_ptr<BranchEdge>& edge = *it;

                    drob_assert(!edge->invalidated);
                    if (edge->dst != b && !edge->dst->visited)
                        blocks.push(edge->dst);
                }
                b = b->getNext();
            } while (b);
        }

        return 0;
    }

    /*
     * Go over all blocks in breadth-first search order.
     */
    int for_each_block_bfs(NodeCallback *cb)
    {
        std::queue<SuperBlock *> blocks;
        int ret;

        if (!entryBlock)
            return 0;
        if (entryBlock->visited)
            mark_all_blocks(false);
        blocks.push(entryBlock);

        while (!blocks.empty()) {
            SuperBlock *b = blocks.front();

            blocks.pop();
            if (b->visited)
                continue;

            /*
             * We treat fallthrough blocks as if it would be
             * one big block. (well it actually is, we just had
             * to split it)
             */
            do {
                if (b->visited) {
                    b = b->getNext();
                    continue;
                }
                b->visited = true;
                ret = cb->handleBlock(b, this);
                if (ret)
                    return ret;

                for (auto && edge: b->getOutgoingEdges()) {
                    drob_assert(!edge->invalidated);
                    if (edge->dst != b && !edge->dst->visited)
                        blocks.push(edge->dst);
                }
                b = b->getNext();
            } while (b);
        }

        return 0;
    }

    /*
     * Go over all instructions in any block order. Instructions
     * will be process sequentially per block. (including unconnected
     * nodes)
     */
    int for_each_instruction_any(NodeCallback *cb)
    {
        int ret;

        for (auto && block : blocks) {
            ret = block->for_each_instruction(cb, this);
            if (ret)
                return ret;
        }
        return 0;
    }

    /*
     * Create a new block by starting to decode at the given itext until
     * the end of the block. Return the new block.
     * Must not be called while iterating over blocks.
     */
    SuperBlock* decodeBlock(const uint8_t *itext, const RewriterCfg& cfg);

    /*
     * Delete a block from the function, along with all instructions and edges.
     * Must not be called while iterating over blocks.
     */
    void removeBlock(SuperBlock *block);

    /*
     * Delete all blocks, along with all instructions and edges.
     * Must not be called while iterating over blocks.
     */
    void removeAllBlocks(void);

    /*
     * Merge a block into its successor (a.k.a. next pointer). Helpful for
     * removing empty blocks. Use with care, will fixup edges but not care
     * if it is logically correct. After this call, the block is deleted.
     * Must not be called while iterating over blocks.
     */
    void mergeBlockIntoNext(SuperBlock *block);

    /*
     * Split a block _at_ the given instruction. The instruction including all
     * following ones will be moved to a new block. A pointer to the new block
     * is returned. All edges and pointers are fixed up. If a nullptr is given,
     * all instructions are moved to the new block.
     * Must not be called while iterating over blocks.
     */
    SuperBlock* splitBlock(SuperBlock *block, Instruction *instruction);

    /*
     * Split a block after the given instruction. All instructions following
     * the given instruction will be moved to a new block. A pointer to the new
     * block is returned. All edges and pointers are fixed up. If a nullptr is
     * given, all instructions are moved to the new block.
     * Must not be called while iterating over blocks.
     */
    SuperBlock* splitBlockAfter(SuperBlock *block, Instruction *instruction);

    /*
     * Create a copy of a block. Return the copy. All outgoing edges are
     * copied and src+dst are fixed up. Prev/Next is not copied. Incoming
     * edges are not touched and not copied. The new block is therefore not
     * reachable but consistent (besides next!).
     */
    SuperBlock* copyBlock(SuperBlock *block);

    const std::vector<std::shared_ptr<CallEdge>>& getIncomingEdges(void) const
    {
        return incomingEdges;
    }

    void addIncomingEdge(const std::shared_ptr<CallEdge> &edge)
    {
        drob_assert(edge->dst == this);
        incomingEdges.push_back(edge);
    }

    void removeIncomingEdge(const CallEdge *edge)
    {
        /* make sure to only erase one instance, we could have two for loops */
        for (auto it = incomingEdges.begin(); it != incomingEdges.end(); it++) {
            if (it->get() == edge) {
                incomingEdges.erase(it);
                return;
            }
        }
        drob_assert_not_reached();
    }

    const std::vector<std::shared_ptr<CallEdge>>& getOutgoingEdges(void) const
    {
        return outgoingEdges;
    }

    void addOutgoingEdge(const std::shared_ptr<CallEdge> &edge)
    {
        drob_assert(edge->src == this);
        outgoingEdges.push_back(edge);
    }

    void removeOutgoingEdge(const CallEdge* edge)
    {
        /* make sure to only erase one instance, we could have two for loops */
        for (auto it = outgoingEdges.begin(); it != outgoingEdges.end(); it++) {
            if (it->get() == edge) {
                outgoingEdges.erase(it);
                return;
            }
        }
        drob_assert_not_reached();
    }

    const std::vector<std::shared_ptr<ReturnEdge>>& getReturnEdges(void) const
    {
        return returnEdges;
    }

    void addReturnEdge(const std::shared_ptr<ReturnEdge> &edge)
    {
        drob_assert(edge->dst == this);
        returnEdges.push_back(edge);
    }

    void removeReturnEdge(const ReturnEdge* edge)
    {
        /* make sure to only erase one instance, we could have two for loops */
        for (auto it = returnEdges.begin(); it != returnEdges.end(); it++) {
            if (it->get() == edge) {
                returnEdges.erase(it);
                return;
            }
        }
        drob_assert_not_reached();
    }

    ICFG *getICFG(void)
    {
        return icfg;
    }
private:
    /*
     * Helper to add a new block. The first added block will be considered the
     * entry.
     */
    void addBlock(std::unique_ptr<SuperBlock> block)
    {
        if (!entryBlock)
            entryBlock = block.get();

        blocks.push_back(std::move(block));
    }

    SuperBlock* splitBlock(SuperBlock *block,
                           const std::list<std::unique_ptr<Instruction>>::iterator &it);

    /* the parent ICFG */
    ICFG *icfg;

    /* starting point of this function */
    const uint8_t *itext{0};

    /* the entry block */
    SuperBlock *entryBlock{nullptr};

    /* all basic blocks in the CFG except the entry */
    std::vector<std::unique_ptr<SuperBlock>> blocks;

    /* incoming edges in any order */
    std::vector<std::shared_ptr<CallEdge>> incomingEdges;

    /* outgoing edges in any order */
    std::vector<std::shared_ptr<CallEdge>> outgoingEdges;

    /* return edges from contained instructions/blocks */
    std::vector<std::shared_ptr<ReturnEdge>> returnEdges;

    /* information about input/output operands */
    std::unique_ptr<FunctionSpecification> info;
};

} /* namespace drob */

#endif /* FUNCTION_HPP */
