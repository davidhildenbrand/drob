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
#ifndef X86_PREDICATE_HPP
#define X86_PREDICATE_HPP

#include "../OpcodeInfo.hpp"

namespace drob {

extern const Predicate pred_B;
extern const Predicate pred_Z;
extern const Predicate pred_S;
extern const Predicate pred_P;
extern const Predicate pred_O;
extern const Predicate pred_NB;
extern const Predicate pred_NZ;
extern const Predicate pred_NS;
extern const Predicate pred_NP;
extern const Predicate pred_NO;
extern const Predicate pred_ECX0;
extern const Predicate pred_RCX0;
extern const Predicate pred_NL;
extern const Predicate pred_L;
extern const Predicate pred_BE;
extern const Predicate pred_LE;
extern const Predicate pred_NBE;
extern const Predicate pred_NLE;

}; /* namespace drob */

#endif /* X86_PREDICATE_HPP */
