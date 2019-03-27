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
#include <cstdio>
#include <iostream>

#include "../Instruction.hpp"
#include "../arch.hpp"
#include "Converter.hpp"
#include "x86.hpp"

extern "C" {
#include "xed-interface.h"
}

using namespace drob;

int drob::arch_setup(void)
{
    xed_tables_init();
    return 0;
}

void drob::arch_teardown(void)
{
}

static void check_segment_override(xed_decoded_inst_t &xedd)
{
    int memops = xed_decoded_inst_number_of_memory_operands(&xedd);
    xed_reg_enum_t seg;

    /*
     * FS/GS override can later eventually be added, for now we'll
     * not support it.
     */
    for(int i = 0; i < memops; i++) {
        seg = xed_decoded_inst_get_seg_reg(&xedd ,i);
        if (seg != XED_REG_INVALID) {
            drob_throw("Segment override is not supported");
        }
    }
}

DecodeRet drob::arch_decode_one(const uint8_t **itext, uint16_t max_ilen,
                                std::list<std::unique_ptr<Instruction>> &instrs,
                                const RewriterCfg &cfg)
{
    xed_error_enum_t xed_error;
    xed_decoded_inst_t xedd;
    uint8_t ilen;

    xed_decoded_inst_zero(&xedd);
    xed_decoded_inst_set_mode(&xedd, XED_MACHINE_MODE_LONG_64,
                  XED_ADDRESS_WIDTH_64b);
    xed_error = xed_decode(&xedd, *itext, max_ilen);

    if (unlikely(xed_error != XED_ERROR_NONE)) {
        drob_error("XED decoding: %s", xed_error_enum_t2str(xed_error));
        return DecodeRet::BrokenInstr;
    }

    ilen = (uint8_t)xed_decoded_inst_get_length(&xedd);
    if (unlikely(loglevel > DROB_LOGLEVEL_DEBUG))
        arch_decode_dump(*itext, *itext + ilen);
    *itext += ilen;

    /* for now, segment override is not supported */
    check_segment_override(xedd);

    /* drop NOPs, we will regenerate if necessary */
    switch (xed_decoded_inst_get_category(&xedd)) {
    case XED_CATEGORY_NOP:
    case XED_CATEGORY_WIDENOP:
        return DecodeRet::NOP;
    default:
        break;
    }
    return convert_decoded(xedd, instrs, cfg);
}

void drob::arch_decode_dump(const uint8_t *start, const uint8_t *end)
{
    xed_error_enum_t xed_error;
    xed_decoded_inst_t xedd;
    char buf[80 + 1];

    if (start >= end) {
        drob_throw("No code to dump");
        return;
    }

    while (start < end) {
        xed_decoded_inst_zero(&xedd);
        xed_decoded_inst_set_mode(&xedd, XED_MACHINE_MODE_LONG_64,
                      XED_ADDRESS_WIDTH_64b);
        xed_error = xed_decode(&xedd, start, end - start);

        if (xed_error != XED_ERROR_NONE) {
            drob_throw("Cannot decode instruction to dump");
            return;
        }

        xed_format_context(XED_SYNTAX_INTEL, &xedd ,buf, sizeof(buf) - 1,
                   (uint64_t)start, nullptr, nullptr);
        buf[80] = 0;
        drob_dump("    %016p: %s", start, buf);
        start += xed_decoded_inst_get_length(&xedd);
    }
}
