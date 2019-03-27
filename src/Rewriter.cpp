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
#include <stack>
#include <queue>
#include <vector>

#include "Utils.hpp"
#include "Rewriter.hpp"
#include "Pass.hpp"
#include "arch.hpp"

using namespace drob;

Rewriter::Rewriter(const uint8_t * itext, const drob_cfg *drob_cfg) :
        cfg(itext, *drob_cfg), memProtCache(*drob_cfg)
{
    /* translate user input into a proper RewriterCfg */
    arch_translate_cfg(*drob_cfg, cfg);

    /* Use a 4MB pool for code and constants for now */
    binaryPool = std::make_unique<BinaryPool>(4 * 1024 * 1024ul);

    /* Create the ICFG */
    passes.emplace_back(new ICFGReconstructionPass(icfg, *binaryPool, cfg, memProtCache));

    if (unlikely(loglevel >= DROB_LOGLEVEL_DEBUG))
        passes.emplace_back(new DumpPass(icfg, *binaryPool, cfg, memProtCache));

    /* TODO: inline functions */

    if (likely(drob_cfg->simple_loop_unroll_count)) {
        /* Perform a simple block-wise unrolling of simple loops */
        passes.emplace_back( new SimpleLoopUnrollingPass(icfg, *binaryPool, cfg,
                                                         memProtCache));

        /* Try to chain and merge blocks */
        passes.emplace_back(new BlockLayoutOptimizationPass(icfg, *binaryPool,
                                                            cfg, memProtCache));

        if (unlikely(loglevel >= DROB_LOGLEVEL_DEBUG))
            passes.emplace_back(new DumpPass(icfg, *binaryPool, cfg, memProtCache));
    }

    /* Remove dead code */
    passes.emplace_back(new DeadCodeEliminationPass(icfg, *binaryPool, cfg, memProtCache));

    /* Try to chain and merge blocks */
    passes.emplace_back(new BlockLayoutOptimizationPass(icfg, *binaryPool, cfg, memProtCache));

    /* Specialize instructions to known input operands */
    passes.emplace_back(new InstructionSpecializationPass(icfg, *binaryPool, cfg, memProtCache));

    /* Optimize memory operands */
    passes.emplace_back(new MemoryOperandOptimizationPass(icfg, *binaryPool, cfg, memProtCache));

    /* Remove dead writes to registers */
    passes.emplace_back(new DeadWriteEliminationPass(icfg, *binaryPool, cfg, memProtCache));

    /* Try to chain and merge blocks */
    passes.emplace_back(new BlockLayoutOptimizationPass(icfg, *binaryPool, cfg, memProtCache));

    if (unlikely(loglevel >= DROB_LOGLEVEL_DEBUG))
        passes.emplace_back(new DumpPass(icfg, *binaryPool, cfg, memProtCache));

    /* Final code generation pass */
    passes.emplace_back(new CodeGenerationPass(icfg, *binaryPool, cfg, memProtCache));

    if (unlikely(loglevel >= DROB_LOGLEVEL_DEBUG))
        passes.emplace_back(new DumpGeneratedCodePass(icfg, *binaryPool, cfg, memProtCache));
}

Rewriter::~Rewriter()
{
}

std::unique_ptr<BinaryPool> Rewriter::rewrite(void)
{
    if (!binaryPool) {
        drob_throw("Rewriter can currently only generate code once");
    }

    /* run all optimization passes, including the code generation pass */
    runPasses();

    return std::move(binaryPool);
}

void Rewriter::runStackAnalysis(void)
{
    StackAnalysisPass analysisPass(icfg, *binaryPool, cfg, memProtCache);

    if (icfg.stackAnalysisValid) {
        drob_debug("-> Stack analysis still valid!");
        return;
    }

    if (likely(analysisPass.needsLivenessAnalysis()))
        runLivenessAnalysis();

    drob_info("%s", std::string(60,'~').c_str());
    drob_info("-> Running analysis: %s (%s)", analysisPass.getName(),
              analysisPass.getDescription());
    drob_info("%s", std::string(60,'~').c_str());
    analysisPass.run();
}

void Rewriter::runLivenessAnalysis(void)
{
    LivenessAnalysisPass analysisPass(icfg, *binaryPool, cfg, memProtCache);

    if (icfg.livenessAnalysisValid) {
        drob_debug("-> Liveness analysis still valid!");
        return;
    }

    drob_info("%s", std::string(60,'~').c_str());
    drob_info("-> Running analysis: %s (%s)", analysisPass.getName(),
              analysisPass.getDescription());
    drob_info("%s", std::string(60,'~').c_str());
    analysisPass.run();
}

void Rewriter::runPass(Pass &pass)
{
    bool rerun = true;
    int iteration = 1;

    drob_info("%s", std::string(60,'#').c_str());
    drob_info("Running pass: %s (%s)", pass.getName(), pass.getDescription());

    while (rerun) {
        if (pass.needsStackAnalysis())
            runStackAnalysis();
        if (pass.needsLivenessAnalysis())
            runLivenessAnalysis();
        drob_info("%s", std::string(60,'*').c_str());
        drob_info("Iteration: %d", iteration);
        drob_info("%s", std::string(60,'*').c_str());
        rerun = pass.run();
        iteration++;
    }
}

void Rewriter::runPasses()
{
    for (auto &&pass : passes) {
        runPass(*pass);
    }
    drob_info("%s", std::string(60,'#').c_str());
}
