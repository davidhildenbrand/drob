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
#ifndef ARCH_HPP
#define ARCH_HPP

#include <cstdint>
#include <list>
#include <queue>
#include <memory>
#include "arch_def.h"
#include "OpcodeInfo.hpp"

namespace drob {

class Instruction;
class BinaryPool;
class ProgramState;
struct CallLocation;
struct BranchLocation;
struct OperandInfo;
struct RegisterInfo;
struct RewriterCfg;

int arch_setup(void);
void arch_teardown(void);

typedef enum class DecodeRet {
    Ok,
    NOP,
    UnhandledInstr,
    UnsupportedInstr,
    BrokenInstr,
    EOB,
} DecodeRet;
DecodeRet arch_decode_one(const uint8_t **itext, uint16_t max_len,
                          std::list<std::unique_ptr<Instruction>> &instrs,
                          const RewriterCfg &cfg);

struct CallLocation {
    /* itext to fix up */
    uint8_t *itext;
    /* ilen to fix up */
    uint8_t ilen;
    /* the call instruction to be written */
    Instruction *instr;
};

struct BranchLocation {
    /* itext to fix up */
    uint8_t *itext;
    /* ilen to fix up */
    uint8_t ilen;
    /* the branch instruction to be written */
    Instruction *instr;
};

void arch_decode_dump(const uint8_t *start, const uint8_t *end);
void arch_fill_with_nops(uint8_t *start, int size);
CallLocation arch_prepare_call(Instruction &instr, BinaryPool &binaryPool);
void arch_fixup_call(const CallLocation &call, const uint8_t *target, bool write);
BranchLocation arch_prepare_branch(Instruction &instr, BinaryPool &binaryPool);
void arch_fixup_branch(const BranchLocation &branch, const uint8_t *target,
                       bool write);
const OpcodeInfo *arch_get_opcode_info(Opcode opc);
const RegisterInfo *arch_get_register_info(Register reg);
/* get by assigned register id, for debugging purposes only - slow */
const RegisterInfo *arch_get_register_info(RegisterType type, int nr);

void arch_translate_cfg(const drob_cfg &drob_cfg, RewriterCfg &cfg);
Opcode arch_invert_branch(Opcode opcode);

} /* namespace drob */

#endif /* ARCH_HPP */
