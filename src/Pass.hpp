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
#ifndef PASS_HPP
#define PASS_HPP

#include "MemProtCache.hpp"
#include "BinaryPool.hpp"
#include "ICFG.hpp"
#include "drob_internal.h"
#include <cerrno>

namespace drob {

class Pass {
public:
    Pass(ICFG &icfg, BinaryPool &binaryPool, const RewriterCfg &cfg,
         const MemProtCache &memProtCache, const char *name,
         const char *description) :
        icfg(icfg), binaryPool(binaryPool), cfg(cfg),
        memProtCache(memProtCache), name(name), description(description)
    {
    }

    /*
     * Get some details (name, description) about a pass.
     */
    const char* getName(void) const
    {
        if (!name)
            return "unnamed";
        return name;
    }

    const char* getDescription(void) const
    {
        if (!description)
            return "unknown";
        return description;
    }

    /*
     * Reset the state (if any).
     */
    virtual void reset(void) { }

    /*
     * Perform the pass on the ICFG.
     */
    virtual bool run(void) = 0;

    /*
     * Is stack analysis necessary?
     */
    virtual bool needsStackAnalysis(void)
    {
        return false;
    }

    /*
     * Is liveness analysis necessary?
     */
    virtual bool needsLivenessAnalysis(void)
    {
        return false;
    }
protected:
    ICFG &icfg;
    BinaryPool &binaryPool;
    const RewriterCfg &cfg;
    const MemProtCache &memProtCache;

private:
    const char* name;
    const char* description;
};

} /* namespace drob */

#include "passes/CodeGenerationPass.hpp"
#include "passes/DumpPass.hpp"
#include "passes/DumpGeneratedCodePass.hpp"
#include "passes/StackAnalysisPass.hpp"
#include "passes/DeadCodeEliminationPass.hpp"
#include "passes/BlockLayoutOptimizationPass.hpp"
#include "passes/ICFGReconstructionPass.hpp"
#include "passes/SimpleLoopUnrollingPass.hpp"
#include "passes/LivenessAnalysisPass.hpp"
#include "passes/DeadWriteEliminationPass.hpp"
#include "passes/InstructionSpecializationPass.hpp"
#include "passes/MemoryOperandOptimizationPass.hpp"

#endif /* PASS_HPP */
