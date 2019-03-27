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
#ifndef MEMPROTCACHE_HPP
#define MEMPROTCACHE_HPP

#include <cstdint>
#include <vector>

#include "Utils.hpp"

namespace drob {

class MemProtCache {
public:
    MemProtCache(const drob_cfg &cfg);

    /* is the given memory range constant (readable but not writable) */
    bool isConstant(uint64_t addr, unsigned long size) const;
    /* is the given memory range is configured to be constant */
    bool isConfiguredConstant(uint64_t addr, unsigned long size) const;
private:
    MemProtCache(const MemProtCache&) = delete;
    MemProtCache &operator=(const MemProtCache &) = delete;

    const drob_cfg &cfg;

    typedef struct MemoryRange {
        uint64_t start;
        uint64_t end;
        uint8_t r : 1;
        uint8_t w : 1;
        uint8_t x : 1;
    } MemoryRange;

    const MemoryRange *findMemoryRange(uint64_t addr) const;

    std::vector<MemoryRange> memoryRanges;
};

} /* namespace drob */

#endif /* MEMPROTCACHE_HPP */
