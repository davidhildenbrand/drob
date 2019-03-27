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
#ifndef X86_HPP
#define X86_HPP

#include "Utils.hpp"

extern "C" {
#include "xed-interface.h"
}

void debug_print_memops(const xed_decoded_inst_t* xedd);
void debug_print_operands(const xed_decoded_inst_t* xedd);

static inline bool is_rel8(int64_t val)
{
    return (int8_t)val == val;
}

static inline bool is_rel32(int64_t val)
{
    return (int32_t)val == val;
}

static inline bool is_simm8(int64_t val)
{
    return (int8_t)val == val;
}

static inline bool is_simm32(int64_t val)
{
    return (int32_t)val == val;
}

static inline bool is_imm32(uint64_t val)
{
    return (uint32_t)val == val;
}

#endif /* X86_HPP */
