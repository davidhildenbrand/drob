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
#include "../Utils.hpp"
#include "Predicate.hpp"

namespace drob {

#define RI(_L0, _C, _L1, _CONN) \
    { \
        .lhs = { .isImm = false, \
             .reg = Register::_L0 }, \
        .comp = PredComparator::_C, \
        .rhs = { .isImm = true, \
             .imm = _L1, }, \
        .con = PredConjunction::_CONN, \
    }

#define RR(_L0, _C, _L1, _CONN) \
    { \
        .lhs = { .isImm = false, \
             .reg = Register::_L0 }, \
        .comp = PredComparator::_C, \
        .rhs = { .isImm = false, \
             .reg = Register::_L1, }, \
        .con = PredConjunction::_CONN, \
    }

#define DEF_PRED_RI(_NAME, _L0, _C0, _L1) \
    const Predicate pred_##_NAME = { \
        .comparisons = { \
            RI(_L0, _C0, _L1, None), \
        }, \
    }

#define DEF_PRED_RR(_NAME, _L0, _C0, _L1) \
    const Predicate pred_##_NAME = { \
        .comparisons = { \
            RR(_L0, _C0, _L1, None), \
        }, \
    }

#define DEF_PRED_RI_RR(_NAME, _L0, _C0, _L1, _CONN, _R0, _C1, _R1) \
    const Predicate pred_##_NAME = { \
        .comparisons = { \
            RI(_L0, _C0, _L1, _CONN), \
            RR(_R0, _C1, _R1, None), \
        }, \
    }

#define DEF_PRED_RI_RI(_NAME, _L0, _C0, _L1, _CONN, _R0, _C1, _R1) \
    const Predicate pred_##_NAME = { \
        .comparisons = { \
            RI(_L0, _C0, _L1, _CONN), \
            RI(_R0, _C1, _R1, None), \
        }, \
    }

DEF_PRED_RI(B, CF, Equal, 1);
DEF_PRED_RI(Z, ZF, Equal, 1);
DEF_PRED_RI(S, SF, Equal, 1);
DEF_PRED_RI(P, PF, Equal, 1);
DEF_PRED_RI(O, OF, Equal, 1);
DEF_PRED_RI(NB, CF, Equal, 0);
DEF_PRED_RI(NZ, ZF, Equal, 0);
DEF_PRED_RI(NS, SF, Equal, 0);
DEF_PRED_RI(NP, PF, Equal, 0);
DEF_PRED_RI(NO, OF, Equal, 0);
DEF_PRED_RI(ECX0, ECX, Equal, 0);
DEF_PRED_RI(RCX0, RCX, Equal, 0);
DEF_PRED_RR(NL, SF, Equal, OF);
DEF_PRED_RR(L, SF, NotEqual, OF);
DEF_PRED_RI_RI(BE, CF, Equal, 1, Or, ZF, Equal, 1);
DEF_PRED_RI_RR(LE, ZF, Equal, 1, Or, SF, NotEqual, OF);
DEF_PRED_RI_RI(NBE, CF, Equal, 0, And, ZF, Equal, 0);
DEF_PRED_RI_RR(NLE, ZF, Equal, 0, And, SF, Equal, OF);

}; /* namespace drob */
