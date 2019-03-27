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
#ifndef SRC_X86_EMULATOR_HPP
#define SRC_X86_EMULATOR_HPP

#include "../OpcodeInfo.hpp"

namespace drob {

emulate_f emulate_add8;
emulate_f emulate_add16;
emulate_f emulate_add32;
emulate_f emulate_add64;
emulate_f emulate_addpd;
emulate_f emulate_addsd;
emulate_f emulate_cmp8;
emulate_f emulate_cmp16;
emulate_f emulate_cmp32;
emulate_f emulate_cmp64;
emulate_f emulate_call;
emulate_f emulate_lea;
emulate_f emulate_mov;
emulate_f emulate_mulpd;
emulate_f emulate_mulsd;
emulate_f emulate_pop;
emulate_f emulate_push;
emulate_f emulate_pxor;
emulate_f emulate_ret;
emulate_f emulate_shl64;
emulate_f emulate_shr64;
emulate_f emulate_sub8;
emulate_f emulate_sub16;
emulate_f emulate_sub32;
emulate_f emulate_sub64;
emulate_f emulate_test8;
emulate_f emulate_test16;
emulate_f emulate_test32;
emulate_f emulate_test64;
emulate_f emulate_xor32;
emulate_f emulate_xor64;

} /* namespace drob */

#endif /* SRC_X86_EMULATOR_HPP */
