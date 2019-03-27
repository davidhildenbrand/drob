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
#ifndef SRC_X86_SPECIALIZE_HPP
#define SRC_X86_SPECIALIZE_HPP

#include "../OpcodeInfo.hpp"

namespace drob {

specialize_f specialize_add64;
specialize_f specialize_addpd;
specialize_f specialize_addsd;

specialize_f specialize_cmp8;
specialize_f specialize_cmp16;
specialize_f specialize_cmp32;
specialize_f specialize_cmp64;

specialize_f specialize_lea64;
specialize_f specialize_lea32;
specialize_f specialize_lea16;

specialize_f specialize_mov32;
specialize_f specialize_mov64;

specialize_f specialize_movapd;
specialize_f specialize_movsd;
specialize_f specialize_movupd;
specialize_f specialize_movups;

specialize_f specialize_mulpd;
specialize_f specialize_mulsd;

specialize_f specialize_pop;

specialize_f specialize_push16;
specialize_f specialize_push64;

specialize_f specialize_pxor;

specialize_f specialize_shl64;
specialize_f specialize_shr64;

specialize_f specialize_test8;
specialize_f specialize_test16;
specialize_f specialize_test32;
specialize_f specialize_test64;

specialize_f specialize_xor32;
specialize_f specialize_xor64;

} /* namespace drob */

#endif /* SRC_X86_SPECIALIZE_HPP */
