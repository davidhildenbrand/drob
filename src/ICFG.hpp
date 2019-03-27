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
#ifndef ICFG_HPP
#define ICFG_HPP

#include <stack>

#include "Instruction.hpp"
#include "Function.hpp"
#include "NodeCallback.hpp"

namespace drob {

class ICFG : public Node {
public:
    ICFG(void) : Node(nullptr) {}

    /*
     * Get the entry function into the ICFG. All other functions
     * can be reached from this function.
     */
    Function *getEntryFunction() const
    {
        return entryFunction;
    }

    /*
     * Set the entry function into the ICFG. It already has to be
     * added to the ICFG.
     */
    void setEntryFunction(Function *function)
    {
        entryFunction = function;
    }

    /*
     * Add a function to the ICFG. The first added function will be
     * considered the entry.
     */
    void addFunction(std::unique_ptr<Function> function)
    {
        if (!entryFunction)
            entryFunction = function.get();
        functions.push_back(std::move(function));
    }

    /*
     * Delete a function from the ICFG, along with all blocks, instructions and
     * edges.
     */
    void removeFunction(Function *function)
    {
        drob_info("Removing function %p (%p)", function,
                  function->getStartAddr());

        if (entryFunction == function)
            entryFunction = nullptr;

        /* remove all instructions along with all edges */
        function->removeAllBlocks();

        auto it = functions.begin();
        for (;it != functions.end(); it++) {
            if ((*it).get() == function) {
                functions.erase(it);
                return;
            }
        }
        drob_assert_not_reached();
    }

    /*
     * Go over all functions in any order (including unconnected nodes).
     */
    int for_each_function_any(NodeCallback *cb)
    {
        int ret;

        for (auto && f : functions) {
            ret = cb->handleFunction(f.get());
            if (ret)
                return ret;
        }
        return 0;
    }

    /*
     * Go over all functions in depth-first search order.
     */
    int for_each_function_dfs(NodeCallback *cb)
    {
        std::stack<Function *> functions;
        int ret;

        if (!entryFunction)
            return 0;
        if (entryFunction->visited)
            mark_all_functions(false);
        functions.push(entryFunction);

        while (!functions.empty()) {
            Function *f = functions.top();

            functions.pop();
            if (f->visited)
                continue;
            f->visited = true;

            ret = cb->handleFunction(f);
            if (ret)
                return ret;

            /*
             * Add in reverse order to the stack so the
             * first call will be processed next.
             */
            auto edges = f->getOutgoingEdges();
            for (auto it = edges.rbegin(); it != edges.rend(); ++it) {
                const std::shared_ptr<CallEdge> &edge = *it;

                drob_assert(!edge->invalidated);
                if (edge->dst != f && !edge->dst->visited)
                    functions.push(edge->dst);
            }
        }
        return 0;
    }

    /*
     * Go over all functions in breadth-first search order.
     */
    int for_each_function_bfs(NodeCallback *cb)
    {
        std::queue<Function *> functions;
        int ret;

        if (!entryFunction)
            return 0;
        if (entryFunction->visited)
            mark_all_functions(false);
        functions.push(entryFunction);

        while (!functions.empty()) {
            Function *f = functions.front();

            functions.pop();
            if (f->visited)
                continue;
            f->visited = true;

            ret = cb->handleFunction(f);
            if (ret)
                return ret;

            /* follow all outgoing edges */
            for (auto && edge: f->getOutgoingEdges()) {
                drob_assert(!edge->invalidated);
                if (edge->dst != f && !f->visited)
                    functions.push(edge->dst);
            }
        }
        return 0;
    }

    /*
     * Go over all blocks in any order (including unconnected nodes).
     */
    int for_each_block_any(NodeCallback *cb)
    {
        int ret;

        for (auto && f : functions) {
            ret = f->for_each_block_any(cb);
            if (ret)
                return ret;
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

        for (auto && f : functions) {
            ret = f->for_each_instruction_any(cb);
            if (ret)
                return ret;
        }
        return 0;
    }

    /*
     * Reset the ICFG, dropping all functions.
     */
    void reset(void)
    {
        entryFunction = nullptr;
        functions.clear();
    }

    /*
     * We might want to
     * - copy functions (e.g. create different flavors)
     */
private:
    /*
     * Mark all functions as visited or not visited.
     */
    void mark_all_functions(bool visited)
    {
        for (auto && f : functions) {
            f->visited = visited;
        }
    }

    /* The entry function */
    Function *entryFunction{nullptr};

    /* All functions in the ICFG, including the entry */
    std::vector<std::unique_ptr<Function>> functions;
};

} /* namespace drob */

#endif /* ICFG_HPP */
