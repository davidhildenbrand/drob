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
#ifndef INSTRUCTIONINFO_HPP
#define INSTRUCTIONINFO_HPP

#include <vector>

#include "OpcodeInfo.hpp"
#include "RegisterInfo.hpp"
#include "ProgramState.hpp"

namespace drob {

typedef struct MemPtr {
    MemPtrType type;
    union {
        Immediate64 addr;
        struct {
            /* base index */
            Data base;
            /* index register */
            Data index;
            /* displacement */
            Data disp;
            /* scale: 1, 2, 4, 8 */
            uint8_t scale;
        } sib;
    };
} MemPtr;

/*
 * A memory access.
 */
typedef struct MemAccess {
    /* The computed pointer value for convenience */
    Data ptrVal;
    MemPtr ptr;
    AccessMode mode;
    MemAccessSize size;
} MemAccess;

/*
 * Operand info describing both a implicit and explicit operands of an
 * instruction. E.g. access modes unknown only based on the opcode were already
 * refined based on static instruction properties like raw operands. Predicates
 * are not modeles as operands.
 *
 * E.g. xor %eax,%eax does not read %eax on this level.
 */
typedef struct OperandInfo : public StaticOperandInfo {
    /* Operand number (across explicit and implicit operands) */
    int nr;
    /* Explicit or implicit operand */
    bool isImpl;
} OperandInfo;

/*
 * Information about an instruction constructed only from static properties
 * like raw explicit operands and implicit operands.
 */
typedef struct InstructionInfo {
    /* nasty instruction, we don't completely know what's going on */
    bool nasty;
    /* implicit and explicit operands, excluding predicates */
    std::vector<OperandInfo> operands;

    /* Helpful for liveness analysis - see InstructionInfo */
    bool mayWriteMem;
    /*
     * All registers read as part of the predicate. These registers will be
     * read even if the instruction won't execute!
     */
    SubRegisterMask predicateRegs;
    /*
     * All registers that will be read (explicit/implicit operands).
     * Includes conditionally read registers but not predicate registers.
     */
    SubRegisterMask readRegs;
    /*
     * All registers that will definitely be written (explicit/implicit operands)
     * if the instruction is executed. Does not include conditional writes.
     */
    SubRegisterMask writtenRegs;
    /*
     * All registers that may be conditionally be written (explicit,
     * implicit operands).
     * E.g.
     * - SHL RAX,0 -> we know EFLAGS won't be written.
     * - SHL RAX,1 -> we know EFLAGS will be written.
     * - SHL RAX,CL -> we don't know if EFLAGS will be written
     *
     * This is important:
     * - Results from previous instructions are not completely
     *   invalidated! They are still relevant for any reader following
     *   this instruction - cannot be optimized out.
     * - This might be used to implement instructions where only some
     *   bits/bytes are modified. E.g. "Set bit 0 to 1". Here, the
     *   other bits have to be forwarded. Modeling this as ReadWrite
     *   is not really the case ("misleading dependency").
     */
    SubRegisterMask condWrittenRegs;
} InstructionInfo;

/*
 * Source / Destination of instructions either target memory (via a pointer)
 * or registers. While register pointers are always known, a pointer might
 * actually not be known. Each ptr has a (hopefully known at runtime) access
 * size - the concept is not separated here.
 */
typedef struct DynamicOperandInfo {
    DynamicOperandInfo(const OperandInfo &operand) :
        type(operand.type), nr(operand.nr), isImpl(operand.isImpl) {}
    OperandType type;
    /* Operand number */
    int nr;
    /* Explicit or implicit operand */
    bool isImpl;
    /*
     * Shortcuts for checking the access mode for reads/writes. If isCondInput
     * is true, also isInput is true.
     */
    bool isInput{false};
    bool isCondInput{false};
    bool isOutput{false};
    bool isCondOutput{false};
    /*
     * If we don't have an immediate, this is the actual register/memory
     * pointer. The input located at that location (eventually) is given below.
     */
    union {
        /* OperandType::Register */
        StaticRegAccess regAcc;
        /* OperandType::MemPtr */
        MemAccess memAcc;
    };
    /*
     * The actual input operand (loaded from register/memory/stack) if this
     * operand is read. If the pointer is unknown, the operand is unknown.
     * Immediates also end up directly here.
     */
    /*
     * Use 0 as default for inputs, so instructions that have an
     * access mode of None after refining, can use the value as
     * immediate - e.g. xor %EAX,%EAX
     */
    Data input{(uint64_t)0};
    /*
     * Use Unknown as default, as that is often the default e.g.
     * for flags in the emulator.
     */
    Data output{DataType::Unknown};
} DynamicOperandInfo;

/*
 * With a given ProgramState, we can directly evaluate pointers to operands.
 * And we can refine access modes and access sizes. Don't access the
 * actual memory pointed to by pointers at this point. E.g. LEA or MOV
 * instructions don't want this.
 *
 * E.g. "shl %rax,%cl" with %cl=0 will not write eflags on this level.
 */
typedef struct DynamicInstructionInfo {
    /* The opcode, e.g. if an emulator needs it */
    Opcode opcode;
    /* evaluated predicate */
    TriState willExecute;
    /* nasty instruction, we don't completely know what's going on */
    bool nasty;
    /* implicit and explicit operands, excluding predicates */
    std::vector<DynamicOperandInfo> operands;
    /* input operand summary to make quicker emulation decisions */
    int numInput : 4;
    int numInputEncodedImm : 4;
    int numInputImm : 4;
    int numInputPtr : 4;
    int numInputStackPtr : 4; /* also counts towards numInputOpPtr */
    int numInputTainted : 4;

    /* Helpful for liveness analysis - see InstructionInfo */
    bool mayWriteMem;
    SubRegisterMask predicateRegs;
    SubRegisterMask readRegs;
    SubRegisterMask writtenRegs;
    SubRegisterMask condWrittenRegs;
} DynamicInstructionInfo;

void dump(const DynamicInstructionInfo &info);
void dump(const DynamicOperandInfo &info);
void dump(const Data &data);

} /* namespace drob */

#endif /* INSTRUCTIONINFO_HPP */
