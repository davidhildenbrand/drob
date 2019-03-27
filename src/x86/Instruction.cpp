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
#include "../Instruction.hpp"
#include "../SuperBlock.hpp"
#include "../BinaryPool.hpp"
#include "../MemProtCache.hpp"
#include "x86.hpp"

using namespace drob;

void debug_print_memops(const xed_decoded_inst_t* xedd) {
    unsigned int i, memops = xed_decoded_inst_number_of_memory_operands(xedd);
    printf("Memory Operands\n");

    for( i=0;i<memops ; i++)   {
        xed_bool_t r_or_w = 0;
        xed_reg_enum_t seg;
        xed_reg_enum_t base;
        xed_reg_enum_t indx;
        printf("  %u ",i);
        if ( xed_decoded_inst_mem_read(xedd,i)) {
            printf("   read ");
            r_or_w = 1;
        }
        if (xed_decoded_inst_mem_written(xedd,i)) {
            printf("written ");
            r_or_w = 1;
        }
        if (!r_or_w) {
            printf("   agen "); // LEA instructions
        }
        seg = xed_decoded_inst_get_seg_reg(xedd,i);
        if (seg != XED_REG_INVALID) {
            printf("SEG= %s ", xed_reg_enum_t2str(seg));
        }
        base = xed_decoded_inst_get_base_reg(xedd,i);
        if (base != XED_REG_INVALID) {
            printf("BASE= %3s/%3s ",
                   xed_reg_enum_t2str(base),
                   xed_reg_class_enum_t2str(xed_reg_class(base)));
        }
        indx = xed_decoded_inst_get_index_reg(xedd,i);
        if (i == 0 && indx != XED_REG_INVALID) {
            printf("INDEX= %3s/%3s ",
                   xed_reg_enum_t2str(indx),
                   xed_reg_class_enum_t2str(xed_reg_class(indx)));
            if (xed_decoded_inst_get_scale(xedd,i) != 0) {
                // only have a scale if the index exists.
                printf("SCALE= %u ",
                       xed_decoded_inst_get_scale(xedd,i));
            }
        }
        if (xed_operand_values_has_memory_displacement(xedd))
        {
            xed_uint_t disp_bits =
                xed_decoded_inst_get_memory_displacement_width(xedd,i);
            if (disp_bits)
            {
                xed_int64_t disp;
                printf("DISPLACEMENT_BYTES= %u ", disp_bits);
                disp = xed_decoded_inst_get_memory_displacement(xedd,i);
                printf("0x" XED_FMT_LX16 " base10=" XED_FMT_LD, disp, disp);
            }
        }
        printf(" ASZ%u=%u\n",
               i,
               xed_decoded_inst_get_memop_address_width(xedd,i));
    }
    printf("  MemopBytes = %u\n",
           xed_decoded_inst_get_memory_operand_length(xedd,0));
}

void debug_print_operands(const xed_decoded_inst_t* xedd) {
    unsigned int i, noperands;
#define TBUFSZ 90
    char tbuf[TBUFSZ];
    const xed_inst_t* xi = xed_decoded_inst_inst(xedd);
    xed_operand_action_enum_t rw;
    xed_uint_t bits;

    printf("Operands\n");
    noperands = xed_inst_noperands(xi);
    printf("#   TYPE               DETAILS        VIS  RW       OC2 BITS BYTES NELEM ELEMSZ   ELEMTYPE   REGCLASS\n");
    printf("#   ====               =======        ===  ==       === ==== ===== ===== ======   ========   ========\n");
    tbuf[0]=0;
    for( i=0; i < noperands ; i++) {
        const xed_operand_t* op = xed_inst_operand(xi,i);
        xed_operand_enum_t op_name = xed_operand_name(op);
        printf("%u %6s ",
               i,xed_operand_enum_t2str(op_name));
        switch(op_name) {
          case XED_OPERAND_AGEN:
          case XED_OPERAND_MEM0:
          case XED_OPERAND_MEM1:
            // we print memops in a different function
            xed_strcpy(tbuf, "(see below)");
            break;
          case XED_OPERAND_PTR:  // pointer (always in conjunction with a IMM0)
          case XED_OPERAND_RELBR: { // branch displacements
              xed_uint_t disp_bits =
                  xed_decoded_inst_get_branch_displacement_width(xedd);
              if (disp_bits) {
                  xed_int32_t disp =
                      xed_decoded_inst_get_branch_displacement(xedd);
                  snprintf(tbuf, TBUFSZ,
                           "BRANCH_DISPLACEMENT_BYTES= %d %08x",
                           disp_bits,disp);
              }
            }
            break;
          case XED_OPERAND_IMM0: { // immediates
              char buf[64];
              const unsigned int no_leading_zeros=0;
              xed_uint_t ibits;
              const xed_bool_t lowercase = 1;
              ibits = xed_decoded_inst_get_immediate_width_bits(xedd);
              if (xed_decoded_inst_get_immediate_is_signed(xedd)) {
                  xed_uint_t rbits = ibits?ibits:8;
                  xed_int32_t x = xed_decoded_inst_get_signed_immediate(xedd);
                  xed_uint64_t y = xed_sign_extend_arbitrary_to_64(
                                              (xed_uint64_t)x, ibits);
                  xed_itoa_hex_ul(buf, y, rbits, no_leading_zeros, 64, lowercase);
              }
              else {
                  xed_uint64_t x =xed_decoded_inst_get_unsigned_immediate(xedd);
                  xed_uint_t rbits = ibits?ibits:16;
                  xed_itoa_hex_ul(buf, x, rbits, no_leading_zeros, 64, lowercase);
              }
              snprintf(tbuf,TBUFSZ,
                       "0x%s(%db)",buf,ibits);
              break;
          }
          case XED_OPERAND_IMM1: { // 2nd immediate is always 1 byte.
              xed_uint8_t x = xed_decoded_inst_get_second_immediate(xedd);
              snprintf(tbuf,TBUFSZ,
                       "0x%02x",(int)x);
              break;
          }
          case XED_OPERAND_REG0:
          case XED_OPERAND_REG1:
          case XED_OPERAND_REG2:
          case XED_OPERAND_REG3:
          case XED_OPERAND_REG4:
          case XED_OPERAND_REG5:
          case XED_OPERAND_REG6:
          case XED_OPERAND_REG7:
          case XED_OPERAND_REG8:
          case XED_OPERAND_BASE0:
          case XED_OPERAND_BASE1:
            {
              xed_reg_enum_t r = xed_decoded_inst_get_reg(xedd, op_name);
              snprintf(tbuf,TBUFSZ,
                       "%s=%s",
                       xed_operand_enum_t2str(op_name),
                       xed_reg_enum_t2str(r));
              break;
            }
          default:
            printf("need to add support for printing operand: %s",
                   xed_operand_enum_t2str(op_name));
            drob_assert_not_reached();
        }
        printf("%21s", tbuf);

        rw = xed_decoded_inst_operand_action(xedd,i);

        printf(" %10s %3s %9s",
               xed_operand_visibility_enum_t2str(
                   xed_operand_operand_visibility(op)),
               xed_operand_action_enum_t2str(rw),
               xed_operand_width_enum_t2str(xed_operand_width(op)));
        bits =  xed_decoded_inst_operand_length_bits(xedd,i);
        printf( "  %3u", bits);
        /* rounding, bits might not be a multiple of 8 */
        printf("  %4u", (bits +7) >> 3);
        printf("    %2u", xed_decoded_inst_operand_elements(xedd,i));
        printf("    %3u", xed_decoded_inst_operand_element_size_bits(xedd,i));

        printf(" %10s",
               xed_operand_element_type_enum_t2str(
                   xed_decoded_inst_operand_element_type(xedd,i)));
        printf(" %10s\n",
               xed_reg_class_enum_t2str(
                   xed_reg_class(
                       xed_decoded_inst_get_reg(xedd, op_name))));
    }
}

const uint8_t *Instruction::getRawTarget(const MemProtCache &memProtCache) const
{
    if (!(isBranch() || isCall())) {
        drob_throw("Trying to resolve target for wrong instruction");
    }

    if (!itext)
        return nullptr;

    switch(this->opcode) {
    case Opcode::JNBEa:
    case Opcode::JNBa:
    case Opcode::JBa:
    case Opcode::JBEa:
    case Opcode::JCXZ32a:
    case Opcode::JCXZ64a:
    case Opcode::JZa:
    case Opcode::JNLEa:
    case Opcode::JNLa:
    case Opcode::JLa:
    case Opcode::JLEa:
    case Opcode::JNZa:
    case Opcode::JNOa:
    case Opcode::JNPa:
    case Opcode::JNSa:
    case Opcode::JOa:
    case Opcode::JPa:
    case Opcode::JSa:
    case Opcode::JMPa:
    case Opcode::CALLa:
        drob_assert(this->rawOperands.op[0].mem.type == MemPtrType::Direct);
        return (const uint8_t *)this->rawOperands.op[0].mem.addr.val;
    case Opcode::JMPr:
    case Opcode::CALLr:
        return nullptr;
    case Opcode::JMPm:
    case Opcode::CALLm: {
        uint64_t addr = rawOperands.op[0].mem.addr.val;

        if (rawOperands.op[0].mem.type != MemPtrType::Direct) {
            return nullptr;
        }

        /*
         * If the 8 byte we are reading cannot be changed, we can go
         * ahead and read it.
         */
        if (memProtCache.isConstant(addr, sizeof(uint64_t))) {
            return (const uint8_t *)(*((uint64_t *)addr));
        }
        return nullptr;
    }
    default:
        drob_assert_not_reached();
    }
}
