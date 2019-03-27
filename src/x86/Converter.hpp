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
#ifndef SRC_X86_CONVERTER_HPP
#define SRC_X86_CONVERTER_HPP

#include <list>
#include <memory>
#include "../arch.hpp"

extern "C" {
#include "xed-interface.h"
}

namespace drob {

class Instruction;

DecodeRet convert_decoded(const xed_decoded_inst_t &xedd,
                          std::list<std::unique_ptr<Instruction>> &instrs,
                          const RewriterCfg &cfg);

} /* namespace drob */

#endif /* SRC_X86_CONVERTER_HPP */
