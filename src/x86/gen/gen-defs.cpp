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
#include <cstdint>
#include <cstdio>
#include "x86/arch_def.h"

using namespace drob;

extern "C" {
#include "util/bitmap.h"
}

#define _NAME_H_ "GEN_DEFS_H"
#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))

#define DEF_GPRS_WITH_HIGH(_l) \
    static SubRegister subregs_##_l##L[] = { \
        SubRegister::_l##_B0, \
    }; \
    static SubRegister subregs_##_l##H[] = { \
        SubRegister::_l##_B1, \
    }; \
    static SubRegister subregs_##_l##X[] = { \
        SubRegister::_l##_B0, \
        SubRegister::_l##_B1, \
    }; \
    static SubRegister subregs_E##_l##X[] = { \
        SubRegister::_l##_B0, \
        SubRegister::_l##_B1, \
        SubRegister::_l##_W1, \
    }; \
    static SubRegister subregs_R##_l##X[] = { \
        SubRegister::_l##_B0, \
        SubRegister::_l##_B1, \
        SubRegister::_l##_W1, \
        SubRegister::_l##_D1, \
    };

#define DEF_GPRS_OLD(_l) \
    static SubRegister subregs_##_l##L[] = { \
        SubRegister::_l##_B0, \
    }; \
    static SubRegister subregs_##_l[] = { \
        SubRegister::_l##_B0, \
        SubRegister::_l##_B1, \
    }; \
    static SubRegister subregs_E##_l[] = { \
        SubRegister::_l##_B0, \
        SubRegister::_l##_B1, \
        SubRegister::_l##_W1, \
    }; \
    static SubRegister subregs_R##_l[] = { \
        SubRegister::_l##_B0, \
        SubRegister::_l##_B1, \
        SubRegister::_l##_W1, \
        SubRegister::_l##_D1, \
    };

#define DEF_GPRS_NEW(_l) \
    static SubRegister subregs_##_l##B[] = { \
        SubRegister::_l##_B0, \
    }; \
    static SubRegister subregs_##_l##W[] = { \
        SubRegister::_l##_B0, \
        SubRegister::_l##_B1, \
    }; \
    static SubRegister subregs_##_l##D[] = { \
        SubRegister::_l##_B0, \
        SubRegister::_l##_B1, \
        SubRegister::_l##_W1, \
    }; \
    static SubRegister subregs_##_l[] = { \
        SubRegister::_l##_B0, \
        SubRegister::_l##_B1, \
        SubRegister::_l##_W1, \
        SubRegister::_l##_D1, \
    };

#define DEF_FLAG(_l) \
    static SubRegister subregs_##_l[] = { \
        SubRegister::_l, \
    };

#define DEF_XMM(_l) \
    static SubRegister subregs_XMM##_l##_D0[] = { \
        SubRegister::XMM##_l##_D0, \
    }; \
    static SubRegister subregs_XMM##_l##_D1[] = { \
        SubRegister::XMM##_l##_D1, \
    }; \
    static SubRegister subregs_XMM##_l##_D2[] = { \
        SubRegister::XMM##_l##_D2, \
    }; \
    static SubRegister subregs_XMM##_l##_D3[] = { \
        SubRegister::XMM##_l##_D3, \
    }; \
    static SubRegister subregs_XMM##_l##_Q0[] = { \
        SubRegister::XMM##_l##_D0, \
        SubRegister::XMM##_l##_D1, \
    }; \
    static SubRegister subregs_XMM##_l##_Q1[] = { \
        SubRegister::XMM##_l##_D2, \
        SubRegister::XMM##_l##_D3, \
    }; \
    static SubRegister subregs_XMM##_l[] = { \
        SubRegister::XMM##_l##_D0, \
        SubRegister::XMM##_l##_D1, \
        SubRegister::XMM##_l##_D2, \
        SubRegister::XMM##_l##_D3, \
    }; \

/* EFLAGS */
static SubRegister subregs_EFLAGS[] = { \
    SubRegister::CF,
    SubRegister::PF,
    SubRegister::AF,
    SubRegister::ZF,
    SubRegister::SF,
    SubRegister::OF,
};
DEF_FLAG(CF);
DEF_FLAG(PF);
DEF_FLAG(AF);
DEF_FLAG(ZF);
DEF_FLAG(SF);
DEF_FLAG(OF);

/* GPRS */
DEF_GPRS_WITH_HIGH(A);
DEF_GPRS_WITH_HIGH(B);
DEF_GPRS_WITH_HIGH(C);
DEF_GPRS_WITH_HIGH(D);
DEF_GPRS_OLD(SI);
DEF_GPRS_OLD(DI);
DEF_GPRS_OLD(BP);
DEF_GPRS_OLD(SP);
DEF_GPRS_NEW(R8);
DEF_GPRS_NEW(R9);
DEF_GPRS_NEW(R10);
DEF_GPRS_NEW(R11);
DEF_GPRS_NEW(R12);
DEF_GPRS_NEW(R13);
DEF_GPRS_NEW(R14);
DEF_GPRS_NEW(R15);

/* XMM registers */
DEF_XMM(0);
DEF_XMM(1);
DEF_XMM(2);
DEF_XMM(3);
DEF_XMM(4);
DEF_XMM(5);
DEF_XMM(6);
DEF_XMM(7);
DEF_XMM(8);
DEF_XMM(9);
DEF_XMM(10);
DEF_XMM(11);
DEF_XMM(12);
DEF_XMM(13);
DEF_XMM(14);
DEF_XMM(15);

typedef struct SubregSpec {
    SubRegister *data;
    uint32_t len;
} SubregSpec;

typedef struct RegisterDefSpec {
    const char *name;
    SubregSpec subregs;
} RegisterDefSpec;

#define REGISTER_DEF_INIT(_name) \
    { \
        .name = "SUBREGISTER_MASK_" #_name, \
        .subregs = { \
            .data = subregs_##_name, \
            .len = ARRAY_SIZE(subregs_##_name), \
        }, \
    }

static RegisterDefSpec register_def[] = {
    REGISTER_DEF_INIT(EFLAGS),
    REGISTER_DEF_INIT(CF),
    REGISTER_DEF_INIT(PF),
    REGISTER_DEF_INIT(AF),
    REGISTER_DEF_INIT(ZF),
    REGISTER_DEF_INIT(SF),
    REGISTER_DEF_INIT(OF),
    REGISTER_DEF_INIT(AL),
    REGISTER_DEF_INIT(AH),
    REGISTER_DEF_INIT(AX),
    REGISTER_DEF_INIT(EAX),
    REGISTER_DEF_INIT(RAX),
    REGISTER_DEF_INIT(BL),
    REGISTER_DEF_INIT(BH),
    REGISTER_DEF_INIT(BX),
    REGISTER_DEF_INIT(EBX),
    REGISTER_DEF_INIT(RBX),
    REGISTER_DEF_INIT(CL),
    REGISTER_DEF_INIT(CH),
    REGISTER_DEF_INIT(CX),
    REGISTER_DEF_INIT(ECX),
    REGISTER_DEF_INIT(RCX),
    REGISTER_DEF_INIT(DL),
    REGISTER_DEF_INIT(DH),
    REGISTER_DEF_INIT(DX),
    REGISTER_DEF_INIT(EDX),
    REGISTER_DEF_INIT(RDX),
    REGISTER_DEF_INIT(SIL),
    REGISTER_DEF_INIT(SI),
    REGISTER_DEF_INIT(ESI),
    REGISTER_DEF_INIT(RSI),
    REGISTER_DEF_INIT(DIL),
    REGISTER_DEF_INIT(DI),
    REGISTER_DEF_INIT(EDI),
    REGISTER_DEF_INIT(RDI),
    REGISTER_DEF_INIT(BPL),
    REGISTER_DEF_INIT(BP),
    REGISTER_DEF_INIT(EBP),
    REGISTER_DEF_INIT(RBP),
    REGISTER_DEF_INIT(SPL),
    REGISTER_DEF_INIT(SP),
    REGISTER_DEF_INIT(ESP),
    REGISTER_DEF_INIT(RSP),
    REGISTER_DEF_INIT(R8B),
    REGISTER_DEF_INIT(R8W),
    REGISTER_DEF_INIT(R8D),
    REGISTER_DEF_INIT(R8),
    REGISTER_DEF_INIT(R9B),
    REGISTER_DEF_INIT(R9W),
    REGISTER_DEF_INIT(R9D),
    REGISTER_DEF_INIT(R9),
    REGISTER_DEF_INIT(R10B),
    REGISTER_DEF_INIT(R10W),
    REGISTER_DEF_INIT(R10D),
    REGISTER_DEF_INIT(R10),
    REGISTER_DEF_INIT(R11B),
    REGISTER_DEF_INIT(R11W),
    REGISTER_DEF_INIT(R11D),
    REGISTER_DEF_INIT(R11),
    REGISTER_DEF_INIT(R12B),
    REGISTER_DEF_INIT(R12W),
    REGISTER_DEF_INIT(R12D),
    REGISTER_DEF_INIT(R12),
    REGISTER_DEF_INIT(R13B),
    REGISTER_DEF_INIT(R13W),
    REGISTER_DEF_INIT(R13D),
    REGISTER_DEF_INIT(R13),
    REGISTER_DEF_INIT(R14B),
    REGISTER_DEF_INIT(R14W),
    REGISTER_DEF_INIT(R14D),
    REGISTER_DEF_INIT(R14),
    REGISTER_DEF_INIT(R15B),
    REGISTER_DEF_INIT(R15W),
    REGISTER_DEF_INIT(R15D),
    REGISTER_DEF_INIT(R15),
    REGISTER_DEF_INIT(XMM0_D0),
    REGISTER_DEF_INIT(XMM0_D1),
    REGISTER_DEF_INIT(XMM0_D2),
    REGISTER_DEF_INIT(XMM0_D3),
    REGISTER_DEF_INIT(XMM0_Q0),
    REGISTER_DEF_INIT(XMM0_Q1),
    REGISTER_DEF_INIT(XMM0),
    REGISTER_DEF_INIT(XMM1_D0),
    REGISTER_DEF_INIT(XMM1_D1),
    REGISTER_DEF_INIT(XMM1_D2),
    REGISTER_DEF_INIT(XMM1_D3),
    REGISTER_DEF_INIT(XMM1_Q0),
    REGISTER_DEF_INIT(XMM1_Q1),
    REGISTER_DEF_INIT(XMM1),
    REGISTER_DEF_INIT(XMM2_D0),
    REGISTER_DEF_INIT(XMM2_D1),
    REGISTER_DEF_INIT(XMM2_D2),
    REGISTER_DEF_INIT(XMM2_D3),
    REGISTER_DEF_INIT(XMM2_Q0),
    REGISTER_DEF_INIT(XMM2_Q1),
    REGISTER_DEF_INIT(XMM2),
    REGISTER_DEF_INIT(XMM3_D0),
    REGISTER_DEF_INIT(XMM3_D1),
    REGISTER_DEF_INIT(XMM3_D2),
    REGISTER_DEF_INIT(XMM3_D3),
    REGISTER_DEF_INIT(XMM3_Q0),
    REGISTER_DEF_INIT(XMM3_Q1),
    REGISTER_DEF_INIT(XMM3),
    REGISTER_DEF_INIT(XMM4_D0),
    REGISTER_DEF_INIT(XMM4_D1),
    REGISTER_DEF_INIT(XMM4_D2),
    REGISTER_DEF_INIT(XMM4_D3),
    REGISTER_DEF_INIT(XMM4_Q0),
    REGISTER_DEF_INIT(XMM4_Q1),
    REGISTER_DEF_INIT(XMM4),
    REGISTER_DEF_INIT(XMM5_D0),
    REGISTER_DEF_INIT(XMM5_D1),
    REGISTER_DEF_INIT(XMM5_D2),
    REGISTER_DEF_INIT(XMM5_D3),
    REGISTER_DEF_INIT(XMM5_Q0),
    REGISTER_DEF_INIT(XMM5_Q1),
    REGISTER_DEF_INIT(XMM5),
    REGISTER_DEF_INIT(XMM6_D0),
    REGISTER_DEF_INIT(XMM6_D1),
    REGISTER_DEF_INIT(XMM6_D2),
    REGISTER_DEF_INIT(XMM6_D3),
    REGISTER_DEF_INIT(XMM6_Q0),
    REGISTER_DEF_INIT(XMM6_Q1),
    REGISTER_DEF_INIT(XMM6),
    REGISTER_DEF_INIT(XMM7_D0),
    REGISTER_DEF_INIT(XMM7_D1),
    REGISTER_DEF_INIT(XMM7_D2),
    REGISTER_DEF_INIT(XMM7_D3),
    REGISTER_DEF_INIT(XMM7_Q0),
    REGISTER_DEF_INIT(XMM7_Q1),
    REGISTER_DEF_INIT(XMM7),
    REGISTER_DEF_INIT(XMM8_D0),
    REGISTER_DEF_INIT(XMM8_D1),
    REGISTER_DEF_INIT(XMM8_D2),
    REGISTER_DEF_INIT(XMM8_D3),
    REGISTER_DEF_INIT(XMM8_Q0),
    REGISTER_DEF_INIT(XMM8_Q1),
    REGISTER_DEF_INIT(XMM8),
    REGISTER_DEF_INIT(XMM9_D0),
    REGISTER_DEF_INIT(XMM9_D1),
    REGISTER_DEF_INIT(XMM9_D2),
    REGISTER_DEF_INIT(XMM9_D3),
    REGISTER_DEF_INIT(XMM9_Q0),
    REGISTER_DEF_INIT(XMM9_Q1),
    REGISTER_DEF_INIT(XMM9),
    REGISTER_DEF_INIT(XMM10_D0),
    REGISTER_DEF_INIT(XMM10_D1),
    REGISTER_DEF_INIT(XMM10_D2),
    REGISTER_DEF_INIT(XMM10_D3),
    REGISTER_DEF_INIT(XMM10_Q0),
    REGISTER_DEF_INIT(XMM10_Q1),
    REGISTER_DEF_INIT(XMM10),
    REGISTER_DEF_INIT(XMM11_D0),
    REGISTER_DEF_INIT(XMM11_D1),
    REGISTER_DEF_INIT(XMM11_D2),
    REGISTER_DEF_INIT(XMM11_D3),
    REGISTER_DEF_INIT(XMM11_Q0),
    REGISTER_DEF_INIT(XMM11_Q1),
    REGISTER_DEF_INIT(XMM11),
    REGISTER_DEF_INIT(XMM12_D0),
    REGISTER_DEF_INIT(XMM12_D1),
    REGISTER_DEF_INIT(XMM12_D2),
    REGISTER_DEF_INIT(XMM12_D3),
    REGISTER_DEF_INIT(XMM12_Q0),
    REGISTER_DEF_INIT(XMM12_Q1),
    REGISTER_DEF_INIT(XMM12),
    REGISTER_DEF_INIT(XMM13_D0),
    REGISTER_DEF_INIT(XMM13_D1),
    REGISTER_DEF_INIT(XMM13_D2),
    REGISTER_DEF_INIT(XMM13_D3),
    REGISTER_DEF_INIT(XMM13_Q0),
    REGISTER_DEF_INIT(XMM13_Q1),
    REGISTER_DEF_INIT(XMM13),
    REGISTER_DEF_INIT(XMM14_D0),
    REGISTER_DEF_INIT(XMM14_D1),
    REGISTER_DEF_INIT(XMM14_D2),
    REGISTER_DEF_INIT(XMM14_D3),
    REGISTER_DEF_INIT(XMM14_Q0),
    REGISTER_DEF_INIT(XMM14_Q1),
    REGISTER_DEF_INIT(XMM14),
    REGISTER_DEF_INIT(XMM15_D0),
    REGISTER_DEF_INIT(XMM15_D1),
    REGISTER_DEF_INIT(XMM15_D2),
    REGISTER_DEF_INIT(XMM15_D3),
    REGISTER_DEF_INIT(XMM15_Q0),
    REGISTER_DEF_INIT(XMM15_Q1),
    REGISTER_DEF_INIT(XMM15),
};

static void set_subregs(bitmap_t bitmap[], SubregSpec spec)
{
    uint32_t i;

    for (i = 0; i < spec.len; i++) {
        bitmap_set_bit(bitmap, static_cast<uint16_t>(spec.data[i]));
    }
}

static void print_mask_define(const char *name, unsigned long mask[],
                  unsigned int len)
{
    unsigned int j;

    printf("#define %s\t", name);

    for (j = 0; j < len; j++) {
        printf("0x%016lxul", mask[j]);
        if (j < len - 1) {
            printf(",");
        } else {
            printf("\n");
        }
    }
}

static void print_register_masks(void)
{
    BITMAP_DECLARE(mask, static_cast<uint16_t>(SubRegister::MAX));
    unsigned int i;

    printf("/* SubRegister masks */\n");

    for (i = 0; i < ARRAY_SIZE(register_def); i++) {
        bitmap_zero(mask, static_cast<uint16_t>(SubRegister::MAX));
        set_subregs(mask, register_def[i].subregs);
        print_mask_define(register_def[i].name, mask, ARRAY_SIZE(mask));
    }

    printf("\n/* Mask for all registers */\n");

    bitmap_fill(mask, static_cast<uint16_t>(SubRegister::MAX));
    print_mask_define("SUBREGISTER_MASK_ALL", mask, ARRAY_SIZE(mask));
}

int main(void)
{
    printf("/*\n"
           " * AUTOMATICALLY GENERATED, DO NOT MODIFY\n"
           " */\n\n"
           "#ifndef " _NAME_H_ "\n#define " _NAME_H_ "\n\n");

    print_register_masks();
    printf("\n#endif\n");
}
