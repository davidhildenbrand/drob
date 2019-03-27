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
#ifndef REWRITER_HPP
#define REWRITER_HPP

#include <memory>
#include <vector>
#include <unordered_map>

#include "MemProtCache.hpp"
#include "BinaryPool.hpp"
#include "ICFG.hpp"
#include "RewriterCfg.hpp"

namespace drob {

class Pass;

class Rewriter {
public:
    Rewriter(const uint8_t * itext, const drob_cfg *cfg);
    ~Rewriter();
    Rewriter(const Rewriter &) = delete;
    Rewriter &operator=(const Rewriter &) = delete;

    /*
     * Rewrite the given function using the given configuration,
     * producing code + constants in the form of a BinaryPool.
     */
    std::unique_ptr<BinaryPool> rewrite(void);
private:
    /* the configuration */
    RewriterCfg cfg;

    /* the binary pool for code and constants */
    std::unique_ptr<BinaryPool> binaryPool;

    /* the cache for memory protections */
    MemProtCache memProtCache;

    /* the icfg we reconstructed */
    ICFG icfg;

    /* the configured optimization passes + final code generation pass */
    std::vector<std::unique_ptr<Pass>> passes;

    /* run a single pass on the ICFG */
    void runPass(Pass &pass);

    /* run all passes on the ICFG */
    void runPasses(void);

    /* run stack analysis */
    void runStackAnalysis(void);

    /* run register liveness analysis */
    void runLivenessAnalysis(void);
};

} /* namespace drob */

#endif /* REWRITER_HPP */
