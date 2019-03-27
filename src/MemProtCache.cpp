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
#include <fstream>
#include <string>
#include <cstdio>
#include <cinttypes>

#include "MemProtCache.hpp"

namespace drob {

MemProtCache::MemProtCache(const drob_cfg &cfg) : cfg(cfg)
{
    std::ifstream maps;
    std::string map;

    maps.open("/proc/self/maps");
    if (!maps.is_open()) {
        return;
    }

    while (std::getline(maps, map)) {
        MemoryRange range;
        const char *str = map.c_str();
        char *endptr;

        range.start = strtoull(str, &endptr, 16);
        endptr++;
        range.end = strtoull(endptr, &endptr, 16);
        endptr++;
        range.r = *endptr == 'r';
        endptr++;
        range.w = *endptr == 'w';
        endptr++;
        range.x = *endptr == 'x';

        memoryRanges.push_back(range);
    }

    maps.close();
}

const MemProtCache::MemoryRange *MemProtCache::findMemoryRange(uint64_t addr) const
{
    /* FIXME: use binary search */
    for (const auto &r : memoryRanges) {
        if (r.start <= addr && addr < r.end) {
            return &r;
        }
    }
    return NULL;
}

bool MemProtCache::isConstant(uint64_t addr, unsigned long size) const
{
    uint64_t end = addr + size;

    if (!size) {
        return true;
    }
    drob_assert(end > addr);

    while (addr < end) {
        const MemoryRange *range = findMemoryRange(addr);

        /* see if we can read without triggering a segfault */
        if (!range || !range->r) {
            return false;
        }

        /* if it is not mapped read-only, query the user configuration */
        if (range->w && !isConfiguredConstant(addr, size)) {
            return false;
        }
        addr = range->end;
    }

    return true;
}


bool MemProtCache::isConfiguredConstant(uint64_t addr, unsigned long size) const
{
    uint64_t end = addr + size;

    if (!size) {
        return true;
    }
    drob_assert(end > addr);
    if (!cfg.range_count) {
        return false;
    }

    while (addr < end) {
        const drob_mem_cfg *mem = nullptr;

        for (int i = 0; i < cfg.range_count; i++) {
            uint64_t start = (uint64_t)cfg.ranges[i].start;
            uint64_t end = start + cfg.ranges[i].size;

            if (start <= addr && addr < end) {
                mem = &cfg.ranges[i];
                break;
            }
        }

        if (!mem) {
            return false;
        }
        addr = (uint64_t)mem->start + mem->size;
    }

    return true;
}

} /* namespace drob */
