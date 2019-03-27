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
#ifndef SRC_X86_OPCODEINFO_HPP
#define SRC_X86_OPCODEINFO_HPP

#include "../OpcodeInfo.hpp"

namespace drob {

extern const OpcodeInfo * const oci[];

refine_f refine_sh;
refine_f refine_xor_rr;

} /* namespace drob */

#endif /* SRC_X86_OPCODEINFO_HPP */
