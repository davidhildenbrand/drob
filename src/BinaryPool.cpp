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
#include <cstring>
#include <iostream>

#include "Utils.hpp"
#include "BinaryPool.hpp"
#include "arch.hpp"

extern "C" {
#include <sys/mman.h>
}

using namespace drob;

BinaryPool::BinaryPool(uint64_t mmapSize) :
    mmapSize(mmapSize), nextInstr(0), nextConst(0)
{
    if (!IS_ALIGNED(mmapSize, ARCH_PAGE_SIZE)) {
        drob_throw("Memory region size not aligned to " +
                     std::to_string(ARCH_PAGE_SIZE));
    } else if (mmapSize > ARCH_MAX_MMAP_SIZE) {
        drob_throw("Memory region size bigger than " +
                     std::to_string(ARCH_MAX_MMAP_SIZE));
    } else if (mmapSize < 2 * ARCH_PAGE_SIZE) {
        drob_throw("Memory region size smaller than " +
                     std::to_string(2 * ARCH_PAGE_SIZE));
    }

    /* reserve the region - this will not allocate or reserve any memory */
    mmapStart = (uint8_t *) mmap(0, mmapSize, PROT_NONE,
                     MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (mmapStart == (uint8_t *)-1) {
        drob_throw("Can't reserve memory region");
    }

    resetCodePool();
    resetConstantPool();
}

BinaryPool::~BinaryPool()
{
    munmap(mmapStart, mmapSize);
}

uint8_t *BinaryPool::allocCode(int size)
{
    /* do we need a fresh page? VMA merging will limit #mmaps */
    if (ALIGN_DOWN(nextInstr + size - 1, ARCH_PAGE_SIZE) != (uint64_t)curInstrPage) {
        uint8_t *tmp = curInstrPage + 1;

        if (tmp == curConstPage) {
            drob_throw("Memory region full");
        }
        curInstrPage = (uint8_t *) mmap((void *)tmp, ARCH_PAGE_SIZE,
                        PROT_READ | PROT_WRITE | PROT_EXEC,
                        MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED,
                        -1, 0);
        if (curInstrPage != tmp) {
            drob_throw("Can't allocate memory for instructions");
        }
    }

    nextInstr += size;
    return nextInstr - size;
}

const uint8_t *BinaryPool::nextCode()
{
    return nextInstr;
}

const uint8_t *BinaryPool::newBlock(bool write)
{
    if (!IS_ALIGNED(nextInstr, ARCH_BLOCK_ALIGN)) {
        uint8_t *aligned = (uint8_t *)ALIGN_UP(nextInstr, ARCH_BLOCK_ALIGN);

        /* pad it with NOPs - we cannot cross page boundaries here */
        if (write) {
            arch_fill_with_nops(nextInstr, aligned - nextInstr);
        }
        nextInstr = aligned;
    }

    return nextInstr;
}

const uint8_t *BinaryPool::allocConstant(__uint128_t val)
{
    const uint8_t *ret;

    if (map128.find(val) != map128.end()) {
        return map128[val];
    }
    ret = allocConstant((const uint8_t *)&val, sizeof(val));
    map128.insert(std::make_pair(val, ret));

    return ret;
}

const uint8_t *BinaryPool::allocConstant(uint64_t val)
{
    const uint8_t *ret;

    if (map64.find(val) != map64.end()) {
        return map64[val];
    }
    ret = allocConstant((const uint8_t *)&val, sizeof(val));
    map64.insert(std::make_pair(val, ret));

    return ret;
}

const uint8_t *BinaryPool::allocConstant(uint32_t val)
{
    return allocConstant((const uint8_t *)&val, sizeof(val));
}

const uint8_t *BinaryPool::allocConstant(uint16_t val)
{
    return allocConstant((const uint8_t *)&val, sizeof(val));
}

const uint8_t *BinaryPool::allocConstant(uint8_t val)
{
    return allocConstant((const uint8_t *)&val, sizeof(val));
}

const uint8_t *BinaryPool::allocConstant(const uint8_t *start, int size)
{
    uint8_t *current;

    drob_assert(IS_POWER_OF_2(size));

    /* TODO: fill holes, merge constants */
    current = (uint8_t *)ALIGN_DOWN(nextConst - size + 1, size);

    /* do we need a fresh page? VMA merging will limit #mmaps */
    if (ALIGN_DOWN(current, ARCH_PAGE_SIZE) != (uint64_t) curConstPage) {
        uint8_t *tmp = curConstPage - 1;

        if (tmp == curInstrPage) {
            drob_throw("Memory region full");
        }
        curConstPage = (uint8_t *) mmap((void *)tmp, ARCH_PAGE_SIZE,
                        PROT_READ | PROT_WRITE,
                        MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED,
                        -1, 0);
        if (curConstPage != tmp) {
            drob_throw("Can't allocate memory for constants");
        }
    }

    /* copy the constant */
    memcpy(current, start, size);
    nextConst = current - 1;

    return current;
}

void BinaryPool::resetCodePool(void)
{
    uint8_t *tmp;

    if (curInstrPage) {
        uint64_t poolSize = curInstrPage + ARCH_PAGE_SIZE - mmapStart;

        tmp = (uint8_t *) mmap((void *)mmapStart, poolSize, PROT_NONE,
                       MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED,
                       -1, 0);
        if (tmp != mmapStart) {
            drob_throw("Cannot reset code pool");
        }
        curInstrPage = nullptr;
        nextInstr = nullptr;
    }

    /* instruction text starts at beginning of memory */
    curInstrPage = (uint8_t *) mmap((void *)mmapStart, ARCH_PAGE_SIZE,
                    PROT_READ | PROT_WRITE | PROT_EXEC,
                    MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED,
                    -1, 0);
    if (curInstrPage != mmapStart) {
        drob_throw("Can't allocate memory for instructions");
    }
    nextInstr = curInstrPage;
}

void BinaryPool::resetConstantPool(void)
{
    uint8_t *tmp;

    map64.clear();
    map128.clear();

    if (curConstPage) {
        uint64_t poolSize = mmapStart + mmapSize - curConstPage;

        tmp = (uint8_t *) mmap(curConstPage, poolSize, PROT_NONE,
                       MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED,
                       -1, 0);
        if (tmp == (uint8_t *)-1) {
            drob_throw("Cannot reset constant pool");
        }
        curConstPage = nullptr;
        nextConst = nullptr;
    }

    /* constant pool starts at end of the reserved memory region */
    tmp = mmapStart + mmapSize - ARCH_PAGE_SIZE;
    curConstPage = (uint8_t *) mmap((void *)tmp, ARCH_PAGE_SIZE,
                    PROT_READ | PROT_WRITE,
                    MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED,
                    -1, 0);
    if (curConstPage != tmp) {
        drob_throw("Can't allocate memory for constants");
    }
    nextConst = curConstPage + ARCH_PAGE_SIZE - 1;
}

void BinaryPool::dump()
{
    arch_decode_dump(mmapStart, nextInstr);
}
