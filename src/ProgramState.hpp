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
#ifndef PROGRAM_STATE_HPP
#define PROGRAM_STATE_HPP

#include <vector>

#include "Utils.hpp"
#include "OpcodeInfo.hpp"
#include "RegisterInfo.hpp"

namespace drob {

/*
 * The type we track. Used both, for elements inside the ProgramState but also
 * for input/output of the ProgramState.
 */
typedef enum class DataType : uint8_t {
    /*
     * The element is dead (never written on any path). Has to be treated
     * similar to Unknown, but might indicate that something strange is
     * going on (e.g. other calling ABI). For moves, it is usually ok.
     */
    Dead = 0,
    /*
     * The element is alive (written on at least one path reaching
     * to this state) but the content is effectively unknown. Sad but true.
     */
    Unknown,
    /*
     * The value is unknown, but it was part of a stack ptr that got
     * partially (or conditionally!) overridden. E.g. modifying the last
     * byte of a stack pointer. If this gets ever used for addressing, some
     * evil stuff is going on. Abort!
     */
    Tainted,
    /*
     * The element contains one byte data. This value can be used
     * independently from others.
     */
    Immediate,
    /* The element is the start of a 8 byte stack ptr + offset */
    StackPtr,
    /* The element is the start of a 8 byte usr ptr + offset */
    UsrPtr,
    /*
     * The element is the start of a 8 byte return pointer to the caller.
     */
    ReturnPtr,

    ///////////////////////////////////////////////////////////////////////////
    /*
     * The following types will never leave the core. They are only used
     * internally. Preserved is special, as it may enter but will always
     * leave as Unknown. These values should only be moved around.
     */
    ///////////////////////////////////////////////////////////////////////////

    /*
     * The element belongs to an successor element (e.g. ptr) and
     * should not be used/modified on its own. If to be modified or read:
     * - Preserved/ReturnPtr/UsrPtr: Reading returns unknown, writing
     *   sets other parts to unknown.
     * - StackPtr: Reading returns UnknownStackPtrPiece, writing
     *   sets other parts to UnknownStackPtrPiece
     */
    Tail,
    StackPtrTail,
    /*
     * The element is the start of a 8 byte preserved area. (other
     * lengths can be supported later on on demand.)
     */
    Preserved8,
} DataType;

static inline bool isDead(DataType type)
{
    return type == DataType::Dead;
}

static inline bool isImm(DataType type)
{
    return type == DataType::Immediate;
}

static inline bool isStackPtr(DataType type)
{
    return type == DataType::StackPtr;
}

static inline bool isTainted(DataType type)
{
    return type == DataType::Tainted;
}

static inline bool isPtr(DataType type)
{
    return type == DataType::StackPtr ||
           type == DataType::UsrPtr ||
           type == DataType::ReturnPtr;
}

static inline bool isPreserved(DataType type)
{
    return type == DataType::Preserved8;
}

/*
 * We track elements on the stack and in registers on a byte basis. Everything
 * else is just madness.
 */
typedef uint8_t ElementData;

/*
 * For each byte of our stack and registers, we need some metadata.
 */
typedef struct ElementMetadata {
    DataType type{DataType::Dead};
    /* TODO: we could use for both a combined uint8_t 4/4 */
    uint8_t nr{0};
} ElementMetadata;

typedef class Data {
public:
    Data(DataType type, unsigned int nr = 0, int64_t ptrOffset = 0) :
        type(type), nr(nr), ptrOffset(ptrOffset)
    {
        drob_assert(ptrOffset == 0 || isPtr());
        drob_assert(!nr || isPtr() || isPreserved(type));
        /* Tail should never leave/enter the core, Preserved may enter. */
        drob_assert(type != DataType::Tail);
        drob_assert(type != DataType::StackPtrTail);
    }
    Data() : type(DataType::Unknown) {}
    Data(uint8_t val) :
        type(DataType::Immediate), size(ImmediateSize::Imm64), imm64_val(val)
    {}
    Data(uint16_t val) :
        type(DataType::Immediate), size(ImmediateSize::Imm64), imm64_val(val)
    {}
    Data(uint32_t val) :
        type(DataType::Immediate), size(ImmediateSize::Imm64), imm64_val(val)
    {}
    Data(uint64_t val) :
        type(DataType::Immediate), size(ImmediateSize::Imm64), imm64_val(val)
    {}
    Data(unsigned __int128 val) :
        type(DataType::Immediate), size(ImmediateSize::Imm128), imm128_val(val)
    {}
    DataType getType(void) const
    {
        return type;
    }
    int64_t getPtrOffset(void) const
    {
        return ptrOffset;
    }
    unsigned int getNr(void) const
    {
        return nr;
    }

    /*
     * An unknown stack pointer or potential pieces of a stack pointer. Any
     * access via this value as a pointer could potentially access the stack.
     */
    bool isTainted(void) const
    {
        return type == DataType::Tainted;
    }
    /*
     * An immediate value.
     */
    bool isImm(void) const
    {
        return type == DataType::Immediate;
    }
    /*
     * An unknown pointer (excluding stackpointers), unknown values or dead
     * values. Dead and unknown values should be treated equally by most code.
     * Dead values are basically unknown.
     */
    bool isUnknownOrDead(void) const
    {
        return type == DataType::Unknown || type == DataType::Dead;
    }
    /*
     * Any pointer.
     */
    bool isPtr(void) const
    {
        return type == DataType::StackPtr || type == DataType::UsrPtr ||
               type == DataType::ReturnPtr;
    }
    /*
     * A stack pointer.
     */
    bool isStackPtr(void) const
    {
        return type == DataType::StackPtr;
    }
    /*
     * A user pointer.
     */
    bool isUsrPtr(void) const
    {
        return type == DataType::UsrPtr;
    }
    /*
     * A return pointer.
     */
    bool isReturnPtr(void) const
    {
        return type == DataType::ReturnPtr;
    }
    bool isImm64(void) const
    {
        return isImm() && size == ImmediateSize::Imm64;
    }
    bool isImm128(void) const
    {
        return isImm() && size == ImmediateSize::Imm128;
    }
    uint64_t getImm64() const
    {
        switch (size) {
        case ImmediateSize::Imm64:
            return imm64_val;
        case ImmediateSize::Imm128:
            return imm128_val;
        default:
            drob_assert_not_reached();
        }
    }
    unsigned __int128 getImm128() const
    {
        switch (size) {
        case ImmediateSize::Imm64:
            return imm64_val;
        case ImmediateSize::Imm128:
            return imm128_val;
        default:
            drob_assert_not_reached();
        }
    }
private:
    typedef enum class ImmediateSize {
        Imm64,
        Imm128,
    } ImmediateSize;

    DataType type{DataType::Unknown};
    union {
        /* ElementType::Immediate */
        ImmediateSize size;
        /* Pointers and preserved values */
        int nr;
    };
    union {
        uint64_t imm64_val;
        unsigned __int128 imm128_val;
        int64_t ptrOffset;
    };
} Data;

typedef class State {
public:
    virtual ElementData& getData(size_t byteOffset) = 0;
    virtual const ElementData& getData(size_t byteOffset) const = 0;
    virtual ElementMetadata& getMetadata(size_t byteOffset) = 0;
    virtual const ElementMetadata& getMetadata(size_t byteOffset) const = 0;
    virtual size_t getSize(void) const = 0;
} ElementsState;

template <unsigned int S>
class RegisterState : public State {
public:
    ElementData& getData(size_t byteOffset)
    {
        return data[byteOffset];
    }
    const ElementData& getData(size_t byteOffset) const
    {
        return data[byteOffset];
    }
    ElementMetadata& getMetadata(size_t byteOffset)
    {
        return metadata[byteOffset];
    }
    const ElementMetadata& getMetadata(size_t byteOffset) const
    {
        return metadata[byteOffset];
    }
    size_t getSize(void) const
    {
        return ARRAY_SIZE(data);
    }
private:
    ElementData data[S];
    ElementMetadata metadata[S];
};

typedef RegisterState<1> Flag1State;
typedef RegisterState<8> Gprs64State;
typedef RegisterState<16> Sse128State;

typedef class StackState : public State {
public:
    void grow(int64_t baseOffset, uint8_t size);

    ElementData& getData(size_t byteOffset)
    {
        if (dead) {
            drob_throw("Stack is dead");
        }
        return data[byteOffset];
    }
    const ElementData& getData(size_t byteOffset) const
    {
        if (dead) {
            drob_throw("Stack is dead");
        }
        return data[byteOffset];
    }
    ElementMetadata& getMetadata(size_t byteOffset)
    {
        if (dead) {
            drob_throw("Stack is dead");
        }
        return metadata[byteOffset];
    }
    const ElementMetadata& getMetadata(size_t byteOffset) const
    {
        if (dead) {
            drob_throw("Stack is dead");
        }
        return metadata[byteOffset];
    }
    size_t getSize(void) const
    {
        if (dead) {
            drob_throw("Stack is dead");
        }
        return data.size();
    }
    int64_t getBase(void) const
    {
        return base;
    }
    void setDead(void)
    {
        drob_debug("Stack set dead");
        dead = true;
        data.resize(0);
        metadata.resize(0);
        base = 0;
    }
    bool isDead(void) const
    {
        return dead;
    }

    /*
     * Stack grows downwards. Old stack is therefore e.g. passed
     * parameters.
     */
    int64_t oldStackSize(void) const
    {
        return data.size() - newStackSize();
    }
    /*
     * New stack is e.g. local variables of the function.
     */
    int64_t newStackSize(void) const
    {
        return base;
    }
    unsigned int getStackIdx(int64_t baseOffset) const
    {
        return base + baseOffset;
    }

private:
    std::vector<ElementData> data;
    std::vector<ElementMetadata> metadata;
    int64_t base{0};
    bool dead{false};
} StackState;

/*
 * The ProgramState tracks registers and the stack on a byte level. It
 * can be used to set/get data of registers and the stack, as well as to
 * directly move from one to the other.
 */
typedef class ProgramState {
public:
    void setRegister(Register reg, RegisterAccessType access,
                     const Data &data, bool cond = false);
    void setRegister(Register reg, const Data &data, bool cond = false)
    {
        setRegister(reg, RegisterAccessType::Full, data, cond);
    }
    void setStack(int64_t baseOffset, MemAccessSize size, const Data &data,
                  bool cond = false);
    Data getStack(int64_t baseOffset, MemAccessSize size) const;
    Data getRegister(Register reg, RegisterAccessType access) const;
    Data getRegister(Register reg) const
    {
        return getRegister(reg, RegisterAccessType::Full);
    }
    void moveStackStack(int64_t baseOffset1, int64_t baseOffset2,
                        MemAccessSize size);
    void moveRegisterRegister(Register reg1, RegisterAccessType access1,
                              Register reg2, RegisterAccessType access2);
    void moveStackRegister(int64_t baseOffset, MemAccessSize size,
                           Register reg, RegisterAccessType access);
    void moveRegisterStack(Register reg, RegisterAccessType access,
                           int64_t baseOffset, MemAccessSize size);
    void nastyInstruction(void);
    void untrackedStackAccess(void)
    {
        stack.setDead();
    }
    bool isStackDead(void) const
    {
        return stack.isDead();
    }
    bool merge(const ProgramState &rhs);
    void dump(void);
private:
    State &getRegisterData(Register reg, RegisterAccessType access,
                       size_t &byteOffs, uint8_t &bytes) const;
    void markUnknown(State &estate, size_t byteOffset,
             uint8_t bytes);
    void clearTail(State &estate, size_t byteOffset);
    void setElements(State &estate, size_t byteOffset, uint8_t bytes,
             const Data &data, bool cond);
    Data getElements(const State &estate, size_t byteOffset,
                  uint8_t bytes) const;
    void moveElements(const State &estate1, size_t byteOffset1,
              State &estate2, size_t byteOffset2,
              uint8_t bytes);
    void setPtr(State &estate, size_t byteOffset, uint8_t bytes,
            const Data &data);
    void setPreserved(State &estate, size_t byteOffset,
              uint8_t bytes, const Data &data);
    void setImm(State &estate, size_t byteOffset, uint8_t bytes,
            const Data &data);
    void setType(State &estate, size_t byteOffset, uint8_t bytes,
             DataType type);
    bool mergeElements(State &lhs, const State &rhs);
    void dumpElements(State &estate, int64_t offset);

    /*
     * The stack starting at the highest touched position, reaching up
     * to the lowest touched position. The stackBase tells us where
     * the stack pointer was pointing to at *entry* of the function.
     *
     * We assume the stack grows downwards, so growing downwards implies
     * adding elemts to the front. Performance wise this is not
     * optimal, but this allows us to directly read/write integers
     * without having to assemble them ourselves from the stack.
     *
     * vector[7]    (ReturnPtr cont.)
     * vector[6]    (ReturnPtr cont.)
     * vector[5]    (ReturnPtr cont.)
     * vector[4]    (ReturnPtr cont.)
     * vector[3]    (ReturnPtr cont.)
     * vector[2]    (ReturnPtr cont.)
     * vector[1]    (ReturnPtr cont.)
     * vector[0] <- ReturnPtr <- stackBase
     *
     * Here, the vector would have a size of 8, stackBase would be 0,
     * Growing the stack (e.g. STACKPTR = STACKPTR - 8) will not change
     * the stackBase. stackBase will only ever change when calling a
     * function. There, we will have to adapt stackBase.
     */
    mutable StackState stack;
    /*
     * The registers, as defined by the architecture.
     */
    mutable Flag1State flag1[ARCH_FLAG1_COUNT];
    mutable Gprs64State gprs64[ARCH_GPRS64_COUNT];
    mutable Sse128State sse128[ARCH_SSE128_COUNT];
} ProgramState;

Data multiplyData(const Data& data, uint8_t scale);
Data addData(const Data &ptr1, const Data &ptr2);

}

#endif /* PROGRAM_STATE_HPP */
