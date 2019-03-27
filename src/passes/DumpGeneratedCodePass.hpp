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
#ifndef PASSES_DUMP_GENERATED_CODE_PASS_HPP
#define PASSES_DUMP_GENERATED_CODE_PASS_HPP

#include "../Pass.hpp"
#include "../NodeCallback.hpp"

namespace drob {

class DumpGeneratedCodePass : public Pass {
public:
    DumpGeneratedCodePass(ICFG &icfg, BinaryPool &binaryPool,
                  const RewriterCfg &cfg, const MemProtCache &memProtCache) :
        Pass(icfg, binaryPool, cfg, memProtCache, "DumpGeneratedCode",
             "Dump the final generated code") {}

    bool run(void)
    {
        binaryPool.dump();
        return false;
    }
};

} /* namespace drob */

#endif /* PASSES_DUMP_GENERATED_CODE_PASS_HPP */
