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
#ifndef OPCODEINFO_HPP
#define OPCODEINFO_HPP

#include "Utils.hpp"
#include "RegisterInfo.hpp"

namespace drob {

class ProgramState;
class DynamicInstructionInfo;
struct LivenessData;
class BinaryPool;
class RewriterCfg;
class ProgramState;
struct OperandInfo;

/*
 * Predicates of instructions are usually pretty simple. We can describe
 * them in an abstract way. E.g. $reg1 == 0 AND $reg2 != $reg2. Other
 * architectures might require some additions.
 */
typedef enum class PredComparator {
    Equal,
    NotEqual,
} PredComparator;

typedef enum class PredConjunction {
    None, /* nothing follows this comparison */
    And,
    Or,
} PredConjunction;

typedef struct PredComparand {
    bool isImm;
    union {
        Register reg;
        uint64_t imm;
    };
} PredComparand;

typedef struct PredComparison {
    PredComparand lhs;
    PredComparator comp;
    PredComparand rhs;
    PredConjunction con;
} PredComparison;

typedef struct Predicate {
    /* for now for x86, 2 comparisons are sufficient */
    PredComparison comparisons[2];
} Predicate;

/*
 * Different operand types.
 */
typedef enum class OperandType {
    None = 0,
    Register,
    MemPtr,
    Immediate8,
    Immediate16,
    Immediate32,
    Immediate64,
    SignedImmediate8,
    SignedImmediate16,
    SignedImmediate32,
    SignedImmediate64,
} OperandType;

/*
 * A memory pointer type. Pretty x86 specific, but can be extended for other
 * architectures.
 *
 * SIB:
 * [base] + [index] * s + disp, used by x86 in different flavors -
 * base or index can be NONE, disp might be 0. When reencoding, we will
 * try to compress the displacement to e.g. 1/2 bytes.
 */
typedef enum class MemPtrType {
    None = 0,
    Direct,
    SIB,
} MemPtrType;

typedef struct Immediate64 {
    uint64_t val;
    /*
     * If, during rewriting, we encode a known UsrPtr value as immediate, we
     * have to make sure to remember the Nr of the UsrPtr + offset, so we don't
     * loose information. Otherwise stack analysis after specializing functions
     * might get crazy.
     *
     * usrPtrNr should always be initialized to -1.
     */
    int usrPtrNr;
    int64_t usrPtrOffset;
} Immediate64;

typedef struct SignedImmediate32 {
    int32_t val;
    /*
     * If, during rewriting, we encode a known UsrPtr value as immediate, we
     * have to make sure to remember the Nr of the UsrPtr + offset, so we don't
     * loose information. Otherwise stack analysis after specializing functions
     * might get crazy.
     *
     * usrPtrNr should always be initialized to -1.
     */
    int usrPtrNr;
    int64_t usrPtrOffset;
} SignedImmediate32;

typedef struct StaticMemPtr {
    MemPtrType type;
    union {
        Immediate64 addr;
        struct {
            /* base index */
            Register base;
            /* index register */
            Register index;
            /* displacement */
            SignedImmediate32 disp;
            /* scale: 1, 2, 4, 8 */
            uint8_t scale;
        } sib;
    };
} StaticMemPtr;

typedef struct StaticOperand {
    union {
        /* OperandType::Register */
        Register reg;
        /* OperandType::Immediate* and OperandType::SignedImmediate* */
        Immediate64 imm;
        /* OperandType::MemPtr */
        StaticMemPtr mem;
    };
} StaticOperand;

typedef struct ExplicitStaticOperands {
    StaticOperand op[ARCH_MAX_OPERANDS];
} ExplicitStaticOperands;

/*
 * Information about a specific opcode. All "real" opcodes share this
 * same information. The data is static and will not change.
 */
typedef enum class AccessMode : uint8_t {
    /* No access (e.g. after refining the access mode) */
    None,
    /* The address is the actual operand (e.g. LEA memory operand, JMP/CALL). */
    Address,
    /* Read only */
    Read,
    /* May read depending on operands (e.g. optimized XOR handling) */
    MayRead,
    /* Write only */
    Write,
    /* May write depending on operands (e.g. conditional write of eflags) */
    MayWrite,
    /* Read and write */
    ReadWrite,
    /* May read and will write (e.g. optimized XOR handling) */
    MayReadWrite,
    /* Will read and may write.*/
    ReadMayWrite,
    /* May read and may write. */
    MayReadMayWrite,
} AccessMode;

static inline bool isRead(AccessMode mode)
{
    switch (mode) {
    case AccessMode::Read:
    case AccessMode::MayRead:
    case AccessMode::ReadWrite:
    case AccessMode::MayReadWrite:
    case AccessMode::ReadMayWrite:
    case AccessMode::MayReadMayWrite:
        return true;
    default:
        return false;
    }
}

static inline bool isWrite(AccessMode mode)
{
    switch (mode) {
    case AccessMode::Write:
    case AccessMode::MayWrite:
    case AccessMode::ReadWrite:
    case AccessMode::MayReadWrite:
    case AccessMode::ReadMayWrite:
    case AccessMode::MayReadMayWrite:
        return true;
    default:
        return false;
    }
}

static inline bool isConditional(AccessMode mode)
{
    switch (mode) {
    case AccessMode::MayRead:
    case AccessMode::MayWrite:
    case AccessMode::MayReadWrite:
    case AccessMode::ReadMayWrite:
    case AccessMode::MayReadMayWrite:
        return true;
    default:
        return false;
    }
}

static inline bool isSpecial(AccessMode mode)
{
    switch (mode) {
    case AccessMode::Address:
        return true;
    default:
        return false;
    }
}

typedef enum class MemAccessSize : uint8_t {
    /* Unknown, e.g. string instructions */
    Unknown = 0,
    /* Number corresponds to the number of bytes */
    B1 = 1,
    B2,
    B3,
    B4,
    B5,
    B6,
    B7,
    B8,
    B9,
    B10,
    B11,
    B12,
    B13,
    B14,
    B15,
    B16,
} MemAccessSize;

typedef struct StaticMemAccess {
    StaticMemPtr ptr;
    AccessMode mode;
    MemAccessSize size;
} StaticMemAccess;

typedef struct StaticRegAccess {
    Register reg;
    AccessMode mode;
    RegisterAccessType r;
    RegisterAccessType w;
} StaticRegAccess;

/*
 * Information about the expected explicit static operand.
 */
typedef struct ExplicitStaticOperandInfo {
    /* The type of operand we expect. */
    OperandType type;
    /* Details depending on the operand type. */
    union {
        struct {
            AccessMode mode;
            MemAccessSize size;
        } m;
        struct {
            RegisterType type;
            AccessMode mode;
            RegisterAccessType r;
            RegisterAccessType w;
        } r;
    };
} ExplicitStaticOperandInfo;

/*
 * Information about an implicit static operand (type, mode, size of access) or
 * an explicit static operand once we know the encoded data (register, immediate).
 */
typedef struct StaticOperandInfo {
    /* The type of operand. */
    OperandType type;
    /* Details depending on the operand type. */
    union {
        /* OperandType::MemPtr */
        StaticMemAccess m;
        /* OperandType::Register */
        StaticRegAccess r;
        /* OperandType::Immediate* and OperandType::SignedImmediate* */
        Immediate64 imm;
    };
} StaticOperandInfo;

/*
 * Very coarse types of opcodes/instructions.
 */
typedef enum OpcodeType : uint8_t {
    Other = 0,
    Ret,
    Call,
    Branch,
} OpcodeType;

typedef enum class EmuRet {
    Ok = 0,
    Mov10,
    Mov02,
    Mov20,
} EmuRet;

typedef enum class SpecRet {
    NoChange = 0,
    Change,
    Delete,
} SpecRet;


/*
 * Refine an operand info (e.g. access mode, access size) based on the actual
 * operands and eventually runtime information.
 */
typedef void (refine_f)(OperandInfo &opInfo, Opcode opcode,
                        const ExplicitStaticOperands &rawOperands, const ProgramState *ps);
typedef int (encode_f)(Opcode opcode, const ExplicitStaticOperands &rawOperands,
                       uint8_t *buf, uint64_t addr);
typedef EmuRet (emulate_f)(DynamicInstructionInfo &dynInfo,
                           const RewriterCfg &cfg);
typedef SpecRet (specialize_f)(Opcode &opcode, ExplicitStaticOperands &rawOperands,
                               const DynamicInstructionInfo &dynInfo,
                               const LivenessData &livenessData,
                               const RewriterCfg &cfg, BinaryPool &binaryPool);

#define DEF_ENCODE_FN(_NAME) \
    int (encode_##_NAME)(Opcode opcode, \
                         const ExplicitStaticOperands &explOperands, \
                         uint8_t *buf, uint64_t addr)
#define DEF_REFINE_FN(_NAME) \
    void (refine_##_NAME)(OperandInfo &operandInfo, Opcode opcode, \
                          const ExplicitStaticOperands &explOperands, \
                          __attribute__((unused)) const ProgramState *ps)
#define DEF_EMULATE_FN(_NAME) \
    EmuRet (emulate_##_NAME)(DynamicInstructionInfo &dynInfo, \
                             __attribute__((unused)) const RewriterCfg &cfg)
#define DEF_SPECIALIZE_FN(_NAME) \
    SpecRet (specialize_##_NAME)(__attribute__((unused)) Opcode &opcode, \
                                 __attribute__((unused)) ExplicitStaticOperands &explOperands, \
                                 __attribute__((unused)) const DynamicInstructionInfo &dynInfo, \
                                 __attribute__((unused)) const LivenessData &livenessData, \
                                 __attribute__((unused)) const RewriterCfg &cfg, \
                                 __attribute__((unused)) BinaryPool &binaryPool)

typedef enum OpcodeFlag {
    OfNone = 0,
    OfEmuImm = 1 << 0,
    OfEmuPtr = 1 << 1,
    OfEmuFull = 1 << 2,
} OpcodeFlag;

/*
 * Information about an opcode.
 */
typedef struct OpcodeInfo {
    /* number of explicit/implicit operands */
    uint8_t numOperands : 4;
    uint8_t numImplOperands : 4;
    /* the type of instruction */
    uint8_t type : 4;
    uint8_t unused : 4;
    /* information about explicit operands */
    const ExplicitStaticOperandInfo *opInfo;
    /* Information about implicit operands */
    const StaticOperandInfo *iOpInfo;
    /* Predicate, if any */
    const Predicate *predicate;
    /* Handler to refine an operand */
    const refine_f *refine;
    /* generate code for a given opcode (some might share the same info) */
    const encode_f *encode;
    /* emulation handler */
    const emulate_f *emulate;
    /* instruction specialization handler */
    const specialize_f *specialize;
    /* flags, e.g. emulation capabilities */
    OpcodeFlag flags;
} OpcodeInfo;

} /* namespace drob */

#endif /* INSTRUCTION_HPP */
