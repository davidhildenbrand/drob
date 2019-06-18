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
#include "ProgramState.hpp"
#include "arch.hpp"

namespace drob {


Data multiplyData(const Data& data, uint8_t scale)
{
    if (scale == 1)
        return data;

    if (data.isImm()) {
        return Data(scale * data.getImm64());
    } else if (data.isStackPtr()) {
        return Data(DynamicValueType::Tainted);
    } else if (data.isPtr()) {
        return Data(DynamicValueType::Unknown);
    }
    return Data(data.getType());
}

Data addData(const Data &ptr1, const Data &ptr2)
{
    Data ptr;

    if (unlikely(ptr1.isImm() && ptr2.isImm())) {
        return Data (ptr1.getImm64() + ptr2.getImm64());
    } else if (ptr1.isPtr() && ptr2.isImm()) {
        return Data(ptr1.getType(), ptr1.getNr(), ptr1.getPtrOffset() + ptr2.getImm64());
    } else if (ptr2.isPtr() && ptr1.isImm()) {
        return Data(ptr2.getType(), ptr2.getNr(), ptr2.getPtrOffset() + ptr1.getImm64());
    } else if (ptr1.isTainted() || ptr2.isTainted() ||
               ptr1.isStackPtr() || ptr2.isStackPtr()) {
        return Data(DynamicValueType::Tainted);
    }
    return Data(DynamicValueType::Unknown);
}

State &ProgramState::getRegisterData(Register reg,
                         RegisterAccessType access,
                         size_t &byteOffset, uint8_t &bytes) const
{
    const RegisterInfo *ri = arch_get_register_info(reg);

    /* Access the parent register */
    if (access == RegisterAccessType::FullZeroParent) {
        reg = ri->parent;
        access = RegisterAccessType::Full;
        ri = arch_get_register_info(reg);
    }

    /* Find the physical storage of the register (via the parent) */
    RegisterType parentType = ri->type;
    uint8_t parentNr = ri->nr;
    if (ri->parent != Register::None) {
        const RegisterInfo *pri = arch_get_register_info(ri->parent);

        parentType = pri->type;
        parentNr = pri->nr;
    }

    /* Detect the offset and size of the access to the register */
    byteOffset = ri->byteOffs;
    bytes = ri->sizeInBytes();

    switch (access) {
    case RegisterAccessType::Full:
        break;
    case RegisterAccessType::H0:
        bytes = bytes / 2;
        break;
    case RegisterAccessType::H1:
        byteOffset += bytes / 2;
        bytes = bytes / 2;
        break;
    case RegisterAccessType::F0:
        bytes = bytes / 4;
        break;
    case RegisterAccessType::F1:
        byteOffset += bytes / 4;
        bytes = bytes / 4;
        break;
    case RegisterAccessType::F2:
        byteOffset += bytes / 2;
        bytes = bytes / 4;
        break;
    case RegisterAccessType::F3:
        byteOffset += 3 * bytes / 4;
        bytes = bytes / 4;
        break;
    default:
        drob_assert_not_reached();
    }

    /* Branch off to the actual physical register */
    switch (parentType) {
    case RegisterType::Flag1:
        return flag1[parentNr];
    case RegisterType::Gprs64:
        return gprs64[parentNr];
    case RegisterType::Sse128:
        return sse128[parentNr];
    default:
        drob_assert_not_reached();
    };
}

void ProgramState::setRegister(Register reg, RegisterAccessType access,
                   const Data &data, bool cond)
{
    size_t byteOffset;
    uint8_t bytes;

    //TODO: handle FullZeroParent properly in cond case
    if (access == RegisterAccessType::FullZeroParent && !cond) {
        State &regState = getRegisterData(reg, access, byteOffset, bytes);

        setElements(regState, byteOffset, bytes, (uint8_t)0, cond);
        access = RegisterAccessType::Full;
    }

    State &regState = getRegisterData(reg, access, byteOffset, bytes);
    setElements(regState, byteOffset, bytes, data, cond);
}

Data ProgramState::getRegister(Register reg, RegisterAccessType access) const
{
    size_t byteOffset;
    uint8_t bytes;

    State &regState = getRegisterData(reg, access, byteOffset, bytes);
    return getElements(regState, byteOffset, bytes);
}

/*
 * See if we have any pointers or preserved values in the range. If so,
 * properly invalidate them.
 */
void ProgramState::markUnknown(State &estate, size_t byteOffset,
                   uint8_t bytes)
{
    for (int i = 0; i < bytes; i++) {
        auto& md = estate.getMetadata(byteOffset + i);

        switch (md.type) {
        case DynamicValueType::Dead:
        case DynamicValueType::Immediate:
            md.type = DynamicValueType::Unknown;
            break;
        case DynamicValueType::Tail:
        case DynamicValueType::StackPtrTail:
            drob_assert(i == 0);
            /* search the parent of the tail and clear it */
            for (int j = -1; ; j--) {
                drob_assert((int)byteOffset + j >= 0);
                auto& md = estate.getMetadata(byteOffset + j);
                if (md.type != DynamicValueType::Tail &&
                    md.type != DynamicValueType::StackPtrTail) {
                    clearTail(estate, byteOffset + j);
                    break;
                }
            }
            break;
        case DynamicValueType::Tainted:
        case DynamicValueType::Unknown:
            break;
        default:
            clearTail(estate, byteOffset);
        }
    }
}

/*
 * When we want to overwrite something that belongs to a pointer (or
 * a preserved value), we have to invalidate the whole thing. Stack pointers
 * are special.
 */
void ProgramState::clearTail(State &estate, size_t byteOffset)
{
    auto &md = estate.getMetadata(byteOffset);

    if (md.type == DynamicValueType::StackPtr)
        md.type = DynamicValueType::Tainted;
    else
        md.type = DynamicValueType::Unknown;

    for (int i = 1; byteOffset + i < estate.getSize(); i++) {
        auto &md = estate.getMetadata(byteOffset + i);

        if (md.type == DynamicValueType::Tail)
            md.type = DynamicValueType::Unknown;
        else if (md.type == DynamicValueType::StackPtrTail)
            md.type = DynamicValueType::Tainted;
        else
            break;
    }
}

void ProgramState::setElements(State &estate, size_t byteOffset,
                   uint8_t bytes, const Data &data, bool cond)
{
    drob_assert(bytes > 0 && bytes + byteOffset <= estate.getSize());

    /*
     * Conditional setting requires special handling, as old values remain
     * also valid. So we have to combine both.
     */
    if (cond) {
        // FIXME: combine data and write combined one instead
        drob_throw("Conditional setting not supported yet.");
    }

    /*
     * Mark everything unknown, which will properly invalidate overlapping
     * pointers.
     */
    markUnknown(estate, byteOffset, bytes);

    switch (data.getType()) {
    case DynamicValueType::StackPtr:
    case DynamicValueType::UsrPtr:
    case DynamicValueType::ReturnPtr:
        setPtr(estate, byteOffset, bytes, data);
        break;
    case DynamicValueType::Immediate:
        setImm(estate, byteOffset, bytes, data);
        break;
    default:
        setType(estate, byteOffset, bytes, data.getType());
        break;
    }
}

Data ProgramState::getElements(const State &estate,
                               size_t byteOffset, uint8_t bytes) const
{
    int hasImm = 0;
    int hasDead = 0;
    int hasPtr = 0;
    int hasStackPtr = 0;
    int hasUnknown = 0;
    int hasTainted = 0;
    bool isMixed = false;

    /* go over all involved elements first */
    for (int i = 0; i < bytes; i++) {
        const auto& md = estate.getMetadata(byteOffset + i);

        switch (md.type) {
        case DynamicValueType::StackPtr:
            hasStackPtr = 1;
            if (i != 0)
                isMixed = true;
            i += 7;
            continue;
        case DynamicValueType::UsrPtr:
        case DynamicValueType::ReturnPtr:
            hasPtr = 1;
            if (i != 0)
                isMixed = true;
            i += 7;
            continue;
        case DynamicValueType::Immediate:
            hasImm = 1;
            continue;
        case DynamicValueType::Dead:
            hasDead = 1;
            continue;
        case DynamicValueType::Tail:
            /* skip over leading tails */
            if (i == 0)
                isMixed = true;
            continue;
        case DynamicValueType::StackPtrTail:
            /* we are reading in the middle of a stack pointer */
            hasTainted = 1;
            continue;
        case DynamicValueType::Unknown:
            hasUnknown = 1;
            continue;
        case DynamicValueType::Tainted:
            hasTainted = 1;
            continue;
        default:
            drob_assert_not_reached();
        };
    }

    /* make sure we catch all mixed cases */
    if (hasImm + hasDead + hasPtr + hasStackPtr + hasUnknown + hasTainted > 1)
        isMixed = true;

    /* if we don't return a complete stack pointer, flag it correctly */
    if (hasTainted || (hasStackPtr &&
        (isMixed || bytes != 8))) {
        return Data(DynamicValueType::Tainted);
    }

    if (!isMixed) {
        const auto& md = estate.getMetadata(byteOffset);
        const auto& data = estate.getData(byteOffset);

        if (hasStackPtr) {
            drob_assert(bytes == 8);
            return Data(md.type, md.nr, *((int64_t *)&data));
        }
        if (hasPtr) {
            if (bytes != 8)
                return Data(DynamicValueType::Unknown);
            /* if we have a complete pointer, return that one */
            return Data(md.type, md.nr, *((int64_t *)&data));
        }
        if (hasImm) {
            switch (bytes) {
            case 1:
                return Data(*((uint8_t *)&data));
            case 2:
                return Data(*((uint16_t *)&data));
            case 4:
                return Data(*((uint32_t *)&data));
            case 8:
                return Data(*((uint64_t *)&data));
            case 16: {
                __uint128_t tmp;

                /* take care of unaligned accesses */
                memcpy(&tmp, &data, sizeof(tmp));
                return Data(tmp);
            }
            default:
                drob_assert_not_reached();
            }
        }
        if (hasDead)
            return Data(DynamicValueType::Dead);
    }
    return Data(DynamicValueType::Unknown);
}

void ProgramState::moveElements(const State &estate1, size_t byteOffset1,
                State &estate2, size_t byteOffset2,
                uint8_t bytes)
{
    if (byteOffset1 == byteOffset2 && &estate1 == &estate2)
        return;

    /*
     * First mark everything unknown, which will properly invalidate
     * overlapping pointers.
     */
    markUnknown(estate2, byteOffset2, bytes);

    for (unsigned int i = 0; i < bytes; i++) {
        const auto& md1 = estate1.getMetadata(byteOffset1 + i);
        const auto& data1 = estate1.getData(byteOffset1 + i);
        auto& md2 = estate2.getMetadata(byteOffset2 + i);
        auto& data2 = estate2.getData(byteOffset2 + i);
        bool isStackPtr = false;

        switch (md1.type) {
        case DynamicValueType::StackPtr:
            isStackPtr = true;
            /* fall through */
        case DynamicValueType::UsrPtr:
        case DynamicValueType::ReturnPtr: {
            if (bytes - i < 8) {
                /* not completely copied :( */
                for (unsigned int j = i; j < bytes; j++) {
                    auto& md = estate2.getMetadata(byteOffset2 + j);
                    if (isStackPtr)
                        md.type = DynamicValueType::Tainted;
                    else
                        md.type = DynamicValueType::Unknown;
                }
            } else {
                /* completely copied :) */
                for (unsigned int j = i; j < i + 8; j++) {
                    const auto& md1 = estate1.getMetadata(byteOffset1 + j);
                    const auto& data1 = estate1.getData(byteOffset1 + j);
                    auto& md2 = estate2.getMetadata(byteOffset2 + j);
                    auto& data2 = estate2.getData(byteOffset2 + j);

                    md2.type = md1.type;
                    md2.nr = md1.nr;
                    data2 = data1;
                }
            }
            i += 7;
            break;
        }
        case DynamicValueType::Immediate:
        case DynamicValueType::Dead:
            md2.type = md1.type;
            data2 = data1;
            break;
        case DynamicValueType::Tail:
        case DynamicValueType::Unknown:
            /* Starting to read in the middle of something */
            md2.type = DynamicValueType::Unknown;
            break;
        case DynamicValueType::StackPtrTail:
        case DynamicValueType::Tainted:
            /* Starting to read in the middle of a stackptr  */
            md2.type = DynamicValueType::Tainted;
            break;
        default:
            drob_assert_not_reached();
        };
    }
}

void ProgramState::setPtr(State &estate, size_t byteOffset,
              uint8_t bytes, const Data &data)
{
    /* Pointers are always 8 bytes long */
    drob_assert(bytes == 8);

    estate.getMetadata(byteOffset).type = data.getType();
    estate.getMetadata(byteOffset).nr = data.getNr();
    for (int i = 1; i < bytes; i++) {
        if (data.isStackPtr())
            estate.getMetadata(byteOffset + i).type = DynamicValueType::StackPtrTail;
        else
            estate.getMetadata(byteOffset + i).type = DynamicValueType::Tail;
    }
    *((int64_t *)&estate.getData(byteOffset)) = data.getPtrOffset();
}

void ProgramState::setImm(State &estate, size_t byteOffset,
              uint8_t bytes, const Data &data)
{
    /* mark all elements as immediates */
    for (int i = 0; i < bytes; i++) {
        estate.getMetadata(byteOffset + i).type = DynamicValueType::Immediate;
    }
    switch (bytes) {
    case 1:
        *((uint8_t *)&estate.getData(byteOffset)) = data.getImm64();
        break;
    case 2:
        *((uint16_t *)&estate.getData(byteOffset)) = data.getImm64();
        break;
    case 4:
        *((uint32_t *)&estate.getData(byteOffset)) = data.getImm64();
        break;
    case 8:
        *((uint64_t *)&estate.getData(byteOffset)) = data.getImm64();
        break;
    case 16:{
        __uint128_t tmp = data.getImm128();

        /* take care of unaligned accesses */
        memcpy(&estate.getData(byteOffset), &tmp, sizeof(tmp));
        break;
    }
    default:
        drob_assert_not_reached();
    }
}

void ProgramState::setType(State &estate, size_t byteOffset,
               uint8_t bytes, DynamicValueType type)
{
    switch (type) {
    case DynamicValueType::Dead:
    case DynamicValueType::Unknown:
    case DynamicValueType::Tainted:
        break;
    default:
        /* Tail is never exposed */
        drob_assert_not_reached();
    }

    for (int i = 0; i < bytes; i++) {
        estate.getMetadata(byteOffset + i).type = type;
        estate.getData(byteOffset + i) = 0;
    }
}

/*
 * Our stack is represented as two arrays. We might have to grow to
 * either sides.
 */
void StackState::grow(int64_t baseOffset, uint8_t size)
{
    int64_t neededOldStackSize = baseOffset + size;
    int64_t neededNewStackSize = -baseOffset;

    if (dead) {
        return;
    }

    /* grow the old part */
    if (neededOldStackSize > oldStackSize()) {
        int64_t vectorSize = newStackSize() + neededOldStackSize;

        data.resize(vectorSize, 0);
        metadata.resize(vectorSize);
    }

    /* grow the new part */
    if (neededNewStackSize > newStackSize()) {
        std::vector<ElementData> newStackData;
        std::vector<ElementMetadata> newStackMetadata;
        int64_t diff = neededNewStackSize - newStackSize();

        /* TODO: We might want to grow bigger at this step */
        newStackData.resize(data.size() + diff, 0);
        newStackMetadata.resize(data.size() + diff);

        std::copy(data.begin(), data.end(),
              newStackData.begin() + diff);
        std::copy(metadata.begin(), metadata.end(),
              newStackMetadata.begin() + diff);
        base += diff;
        data = newStackData;
        metadata = newStackMetadata;
    }
}

void ProgramState::setStack(int64_t baseOffset, MemAccessSize size,
                const Data &data, bool cond)
{
    uint8_t bytes = static_cast<uint8_t>(size);
    drob_assert(size != MemAccessSize::Unknown);

    if (isStackDead()) {
        /* goes directly into the trash */
        return;
    }

    /* first grow the stack if necessary, will be initialized to DEAD */
    stack.grow(baseOffset, bytes);

    setElements(stack, stack.getStackIdx(baseOffset), bytes, data, cond);
}

Data ProgramState::getStack(int64_t baseOffset, MemAccessSize size) const
{
    uint8_t bytes = static_cast<uint8_t>(size);
    drob_assert(size != MemAccessSize::Unknown);

    if (isStackDead()) {
        return Data(DynamicValueType::Tainted);
    }

    /*
     * Fixme: Growing the stack can be avoided by handling all accesses
     * out of range as accesses to DEAD.
     */
    stack.grow(baseOffset, bytes);

    return getElements(stack, stack.getStackIdx(baseOffset), bytes);
}

void ProgramState::moveStackStack(int64_t baseOffset1, int64_t baseOffset2,
                  MemAccessSize size)
{
    uint8_t bytes = static_cast<uint8_t>(size);
    drob_assert(size != MemAccessSize::Unknown);

    if (isStackDead()) {
        /* goes directly into the trash */
        return;
    }

    /*
     * Fixme: Growing the stack can be avoided by handling all accesses
     * out of range as accesses to DEAD.
     */
    stack.grow(baseOffset1, bytes);
    stack.grow(baseOffset2, bytes);

    moveElements(stack, stack.getStackIdx(baseOffset1), stack,
             stack.getStackIdx(baseOffset2), bytes);
}

void ProgramState::moveRegisterRegister(Register reg1, RegisterAccessType access1,
                    Register reg2, RegisterAccessType access2)
{
    size_t byteOffset1, byteOffset2;
    uint8_t bytes1, bytes2;
    State &reg1State = getRegisterData(reg1, access1, byteOffset1, bytes1);

    if (access2 == RegisterAccessType::FullZeroParent) {
        setRegister(reg2, access2, (uint8_t)0, false);
        access2 = RegisterAccessType::Full;
    }
    State &reg2State = getRegisterData(reg2, access2, byteOffset2, bytes2);

    drob_assert(bytes1 == bytes2);
    moveElements(reg1State, byteOffset1, reg2State, byteOffset2, bytes1);
}

void ProgramState::moveStackRegister(int64_t baseOffset, MemAccessSize size,
                     Register reg, RegisterAccessType access)
{
    size_t byteOffset2;
    uint8_t bytes1 = static_cast<uint8_t>(size), bytes2;

    if (access == RegisterAccessType::FullZeroParent) {
        setRegister(reg, access, (uint8_t)0, false);
        access = RegisterAccessType::Full;
    }
    State &regState = getRegisterData(reg, access, byteOffset2, bytes2);

    if (isStackDead()) {
        setRegister(reg, access, Data(DynamicValueType::Tainted));
        return;
    }

    /*
     * Fixme: Growing the stack can be avoided by handling all accesses
     * out of range as accesses to DEAD.
     */
    stack.grow(baseOffset, bytes1);

    drob_assert(bytes1 == bytes2);
    moveElements(stack, stack.getStackIdx(baseOffset), regState,
             byteOffset2, bytes1);
}

void ProgramState::moveRegisterStack(Register reg, RegisterAccessType access,
                     int64_t baseOffset, MemAccessSize size)
{
    size_t byteOffset1;
    uint8_t bytes1, bytes2 = static_cast<uint8_t>(size);

    if (isStackDead()) {
        /* goes directly into the trash */
        return;
    }
    State &regState = getRegisterData(reg, access, byteOffset1, bytes1);

    /*
     * Fixme: Growing the stack can be avoided by handling all accesses
     * out of range as accesses to DEAD.
     */
    stack.grow(baseOffset, bytes2);

    drob_assert(bytes1 == bytes2);
    moveElements(regState, byteOffset1, stack,
             stack.getStackIdx(baseOffset), bytes1);
}

void ProgramState::nastyInstruction(void)
{
    unsigned int i;

    /*
     * Anything could have happened. Taint everything (unless flags, we
     * should be fine with unknown).
     */
    stack.setDead();
    for (i = 0; i < ARRAY_SIZE(flag1); i++) {
        setElements(flag1[i], 0, 1, DynamicValueType::Unknown, false);
    }
    for (i = 0; i < ARRAY_SIZE(gprs64); i++) {
        setElements(gprs64[i], 0, 8, DynamicValueType::Tainted, false);
    }
    for (i = 0; i < ARRAY_SIZE(sse128); i++) {
        setElements(sse128[i], 0, 16, DynamicValueType::Tainted, false);
    }
}

bool ProgramState::mergeElements(State &lhs, const State &rhs)
{
    bool diff = false;
    unsigned int i;

    drob_assert(lhs.getSize() == rhs.getSize());

    for (i = 0; i < lhs.getSize(); i++) {
        auto& lmd = lhs.getMetadata(i);
        auto& ldata = lhs.getData(i);
        auto& rmd = rhs.getMetadata(i);
        auto& rdata = rhs.getData(i);

        /* types match, take a look at the details */
        if (lmd.type == rmd.type) {
            if (isPtr(lmd.type)) {
                /* numbers or offsets don't match */
                if (lmd.nr != rmd.nr ||
                    *((uint64_t *)&ldata) != *((uint64_t *)&rdata)) {
                    diff = true;
                    clearTail(lhs, i);
                }
                i += 7;
            } else if (isImm(lmd.type)) {
                if (ldata != rdata) {
                    diff = true;
                    lmd.type = DynamicValueType::Unknown;
                }
                /* else immediate matches */
            }
        } else if ((lmd.type == DynamicValueType::Dead &&
                   rmd.type == DynamicValueType::Unknown) ||
                   (lmd.type == DynamicValueType::Unknown &&
                    rmd.type == DynamicValueType::Dead)) {
            /*
             * Dead is basically considered unknown. So
             * let's consider them here as dead and don't
             * indicate a change.
             */
            lmd.type = DynamicValueType::Dead;
        } else {
            /*
             * Tie breaker: We must only indicate a diff if the lhs
             * actually changed. Otherwise we could loop forever e.g. trying to
             * combine immediates with unknown values.
             */
            if (lmd.type == DynamicValueType::StackPtr ||
                lmd.type == DynamicValueType::StackPtrTail ||
                lmd.type == DynamicValueType::Tainted ||
                rmd.type == DynamicValueType::StackPtr ||
                rmd.type == DynamicValueType::StackPtrTail ||
                rmd.type == DynamicValueType::Tainted) {
                if (lmd.type != DynamicValueType::Tainted) {
                    diff = true;
                    lmd.type = DynamicValueType::Tainted;
                }
            } else if (lmd.type != DynamicValueType::Unknown) {
                diff = true;
                lmd.type = DynamicValueType::Unknown;
            }
        }
    }
    return diff;
}

bool ProgramState::merge(const ProgramState &rhs)
{
    bool diff = false;
    unsigned int i;

    /* merge the registers */
    for (i = 0; i < ARRAY_SIZE(flag1); i++) {
        diff |= mergeElements(flag1[i], rhs.flag1[i]);
    }
    for (i = 0; i < ARRAY_SIZE(gprs64); i++) {
        diff |= mergeElements(gprs64[i], rhs.gprs64[i]);
    }
    for (i = 0; i < ARRAY_SIZE(sse128); i++) {
        diff |= mergeElements(sse128[i], rhs.sse128[i]);
    }

    /* merge the stacks */
    if (stack.isDead() && !rhs.stack.isDead()) {
        /* no diff: LHS is already weaker than RHS */
    } else if (!stack.isDead() && rhs.stack.isDead()) {
        diff = true;
        stack.setDead();
    } else if (!stack.isDead() && !rhs.stack.isDead()) {
        /*
         * TODO: for simplicity, we'll grow both, the lhs and the rhs.
         */
        int64_t oldStackSize = std::max(stack.oldStackSize(),
                                        rhs.stack.oldStackSize());
        int64_t newStackSize = std::max(stack.newStackSize(),
                                        rhs.stack.newStackSize());
        stack.grow(oldStackSize, 0);
        stack.grow(-newStackSize, 0);
        rhs.stack.grow(oldStackSize, 0);
        rhs.stack.grow(-newStackSize, 0);
        diff |= mergeElements(stack, rhs.stack);
    }
    return diff;
}

void ProgramState::dumpElements(State &estate, int64_t offset)
{
    unsigned int i;

    for (i = 0; i < estate.getSize(); i++) {
        auto& md = estate.getMetadata(i);
        auto& data = estate.getData(i);
        bool merge = false;

        switch (md.type) {
        case DynamicValueType::Dead:
            drob_dump("    %8d: Dead", i - offset);
            merge = true;
            break;
        case DynamicValueType::Unknown:
            drob_dump("    %8d: Unknown", i - offset);
            merge = true;
            break;
        case DynamicValueType::Tainted:
            drob_dump("    %8d: Tainted", i - offset);
            merge = true;
            break;
        case DynamicValueType::Immediate:
            drob_dump("    %8d: %x", i - offset, data);
            break;
        case DynamicValueType::StackPtr:
            drob_dump("    %8d: StackPtr(%d) + %" PRIi64, i - offset,
                  md.nr, *((int64_t *)&data));
            i += 7;
            break;
        case DynamicValueType::ReturnPtr:
            drob_dump("    %8d: ReturnPtr(%d) + %" PRIi64, i - offset,
                  md.nr, *((int64_t *)&data));
            i += 7;
            break;
        case DynamicValueType::UsrPtr:
            drob_dump("    %8d: UsrPtr(%d) + %" PRIi64, i - offset,
                  md.nr, *((int64_t *)&data));
            i += 7;
            break;
        case DynamicValueType::StackPtrTail:
        case DynamicValueType::Tail:
            drob_dump("    %8d: ERROR TAIL", i - offset);
            merge = true;
            break;
        default:
            drob_assert_not_reached();
        }

        if (merge) {
            i++;
            while (i < estate.getSize() &&
                   estate.getMetadata(i).type == md.type)
                i++;
            i--;
        }
    }
}

void ProgramState::dump(void)
{
    const RegisterInfo *ri;
    unsigned int i;

    for (i = 0; i < ARRAY_SIZE(flag1); i++) {
        ri = arch_get_register_info(RegisterType::Flag1, i);
        drob_dump("Flag1(%d): %s", i, ri->name);
        dumpElements(flag1[i], 0);
    }
    for (i = 0; i < ARRAY_SIZE(gprs64); i++) {
        ri = arch_get_register_info(RegisterType::Gprs64, i);
        drob_dump("Gprs64(%d): %s", i, ri->name);
        dumpElements(gprs64[i], 0);
    }
    for (i = 0; i < ARRAY_SIZE(sse128); i++) {
        ri = arch_get_register_info(RegisterType::Sse128, i);
        drob_dump("Sse128(%d): %s", i, ri->name);
        dumpElements(sse128[i], 0);
    }
    drob_dump("Stack");
    if (stack.isDead()) {
        drob_dump("           The stack is dead");
    } else {
        dumpElements(stack, stack.getBase());
    }
}

} /* namespace drob */
