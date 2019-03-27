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
#ifndef SRC_X86_ENCODER_HPP
#define SRC_X86_ENCODER_HPP

#include "../OpcodeInfo.hpp"

namespace drob {

encode_f encode_add;
encode_f encode_addpd;
encode_f encode_addsd;
encode_f encode_call;
encode_f encode_cmp;
encode_f encode_jcc;
encode_f encode_jmp;
encode_f encode_lea;
encode_f encode_mov;
encode_f encode_movapd;
encode_f encode_movsd;
encode_f encode_movupd;
encode_f encode_movups;
encode_f encode_mulpd;
encode_f encode_mulsd;
encode_f encode_pop;
encode_f encode_push;
encode_f encode_pxor;
encode_f encode_ret;
encode_f encode_shl;
encode_f encode_shr;
encode_f encode_sub;
encode_f encode_test;
encode_f encode_xor;

} /* namespace drob */

#endif /* SRC_X86_ENCODER_HPP */
