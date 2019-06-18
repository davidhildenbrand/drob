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
#ifndef REWRITER_CFG_HPP
#define REWRITER_CFG_HPP

#include <vector>

#include "ProgramState.hpp"
#include "Utils.hpp"

namespace drob {

typedef struct UsrPtrCfg {
    bool isKnown{false};
    bool isConst{false};
    bool isRestrict{false};
    bool isNotNull{false};
    uint16_t align;
    const void *val;
} UsrPtrCfg;

/*
 * A portion of the stack, relative to Ptr::Stack
 */
typedef struct StackRange {
    /* Offset from Ptr::Stack */
    int offset{0};
    /* Length in bytes, starting from offset */
    int length{0};
} StackRange;

typedef struct StackOperands {
    std::vector<StackRange> in;
    /*
     * The ABIs we're looking at don't return values via the stack.
     */
} StackOperands;

typedef struct RegisterOperands {
    SubRegisterMask in;
    SubRegisterMask out;
    SubRegisterMask preserved;
} RegisterOperands;

/*
 * A function has a set of input parameters (passed via register/stack)
 * and produces an output (usually via register and not the stack).
 * Other memory accesses to globals / via pointers can of course happen,
 * but that can usually not be optimized out.
 *
 * This information is generated from the user input. Especially, this
 * is just the specification and does not actually represent what a
 * function with given inputs will actually read/write. E.g. if one
 * parameter has a specific value, other parameters might not be read. This
 * is the super set of inputs that might get consumed and outputs that might
 * get produced.
 *
 * However, this is a starting point, as e.g. return value detection
 * usually require knowledge about calling code. For internally called
 * functions, we will later actually have to detect parameters/return
 * operands or inline completely and optimize.
 */
typedef struct FunctionSpecification {
    StackOperands stack;
    RegisterOperands reg;
} FunctionSpecification;

/*
 * The drob_cfg must not be modified, however we have to store and remember
 * some global data. Therefore, we have the RewriterCfg which can be
 * passed around.
 */
typedef class RewriterCfg {
public:
    RewriterCfg(const uint8_t *itext, const drob_cfg& drobCfg) :
            itext(itext), drobCfg(drobCfg)
    {
    }

    const uint8_t *getItext(void) const
    {
        return itext;
    }
    const drob_cfg& getDrobCfg(void) const
    {
        return drobCfg;
    }
    FunctionSpecification &getEntrySpec(void)
    {
        return entrySpec;
    }
    const FunctionSpecification &getEntrySpec(void) const
    {
        return entrySpec;
    }
    ProgramState &getEntryState(void)
    {
        return entryState;
    }
    const ProgramState &getEntryState(void) const
    {
        return entryState;
    }
    unsigned int nextUsrPtr(void)
    {
        usrPtrCfg.emplace_back();
        return usrPtrCfg.size() - 1;
    }
    const UsrPtrCfg &getUsrPtrCfg(unsigned int nr) const
    {
        drob_assert(nr < usrPtrCfg.size());

        return usrPtrCfg[nr];
    }
    UsrPtrCfg &getUsrPtrCfg(unsigned int nr)
    {
        drob_assert(nr < usrPtrCfg.size());

        return usrPtrCfg[nr];
    }
private:
    const uint8_t *itext;
    FunctionSpecification entrySpec;
    ProgramState entryState;
    std::vector<UsrPtrCfg> usrPtrCfg;
    const drob_cfg& drobCfg;
} RewriterCfg;

static inline bool ptrToInt(const DynamicValue &ptr, const RewriterCfg &cfg,
                            uint64_t *ptrVal)
{
    if (ptr.isImm()) {
        *ptrVal = ptr.getImm64();
        return true;
    } else if (ptr.isUsrPtr()) {
        const UsrPtrCfg ptrCfg = cfg.getUsrPtrCfg(ptr.getNr());

        if (ptrCfg.isKnown) {
            *ptrVal = ((int64_t)ptrCfg.val + ptr.getPtrOffset());
            return true;
        }
    }
    return false;
}

} /* namespace drob */

#endif /* REWRITER_CFG_HPP */
