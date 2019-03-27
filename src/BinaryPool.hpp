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
#ifndef BINARYPOOL_HPP
#define BINARYPOOL_HPP

#include "Utils.hpp"
#include <map>

namespace drob {

class BinaryPool {
public:
    BinaryPool(uint64_t mmapSize);
    ~BinaryPool();

    const uint8_t *getEntry(void) const
    {
        if (mmapStart >= nextInstr) {
            throw std::runtime_error("No code generated");
        }
        return mmapStart;
    }

    /* fixme: replace getEntry() */
    const uint8_t *getStartAddr(void) const
    {
        return mmapStart;
    }

    const uint8_t *getEndAddr(void) const
    {
        return mmapStart + mmapSize - 1;
    }

    bool isAddrContained(uint64_t addr) const
    {
        return addr >= (uint64_t)getStartAddr() &&
               addr <= (uint64_t)getEndAddr();
    }

    size_t getCodeSize(void) const
    {
        return (size_t)nextInstr - (size_t)mmapStart;
    }

    size_t getConstantPoolSize(void) const
    {
        return (size_t)mmapStart + (size_t)mmapSize - 1 - (uint64_t)nextConst;
    }

    /*
     * Allocate memory for the next sequential instruction of the given
     * size.
     */
    uint8_t *allocCode(int size);

    /*
     * Get the start of the next instruction without allocating.
     */
    const uint8_t *nextCode(void);

    /*
     * Align the memory for the next sequential instruction to the
     * block size and pad it with NOPs.
     */
    const uint8_t *newBlock(bool write);

    /*
     * Move the constant to the constant pool and return the address to the
     * new constant. Constants might get merged by content.
     */
    const uint8_t *allocConstant(__uint128_t val);
    const uint8_t *allocConstant(uint64_t val);
    const uint8_t *allocConstant(uint32_t val);
    const uint8_t *allocConstant(uint16_t val);
    const uint8_t *allocConstant(uint8_t val);
    const uint8_t *allocConstant(const uint8_t *addr, int size);

    /*
     * Zap it so we can refill.
     */
    void resetCodePool(void);
    void resetConstantPool(void);

    void dump(void);
public:
    uint8_t *mmapStart{nullptr};
    uint64_t mmapSize{0};

    /* current page for instructions, grows upwards */
    uint8_t *curInstrPage{nullptr};
    /* pointer at lower limit of next instruction - starts at beginning of page */
    uint8_t *nextInstr{nullptr};

    /* current page for constants, grows downwards */
    uint8_t *curConstPage{nullptr};
    /* pointer at upper limit of next constant - starts at end of page */
    uint8_t *nextConst{nullptr};

    /*
     * Merge some constants sizes that will appear more frequently and can
     * usually not be encoded as an immediate.
     */
    std::map<uint64_t, const uint8_t *> map64;
    std::map<__uint128_t, const uint8_t *> map128;
};

} /* namespace drob */

#endif /* BINARYPOOL_HPP */
