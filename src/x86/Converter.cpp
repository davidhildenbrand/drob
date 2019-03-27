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
#include "../Instruction.hpp"
#include "../RewriterCfg.hpp"
#include "OpcodeInfo.hpp"
#include "Converter.hpp"

namespace drob {

static inline const OpcodeInfo *getOpcodeInfo(Opcode oc)
{
    return oci[static_cast<uint16_t>(oc)];
}

#define RETURN_OPCODE(_oc) \
    *opcode = Opcode::_oc; \
    return getOpcodeInfo(Opcode::_oc)

static Register translate_reg(xed_reg_enum_t reg)
{
    switch (reg) {
    case XED_REG_RAX:
        return Register::RAX;
    case XED_REG_EAX:
        return Register::EAX;
    case XED_REG_AX:
        return Register::AX;
    case XED_REG_AH:
        return Register::AH;
    case XED_REG_AL:
        return Register::AL;
    case XED_REG_RBX:
        return Register::RBX;
    case XED_REG_EBX:
        return Register::EBX;
    case XED_REG_BX:
        return Register::BX;
    case XED_REG_BH:
        return Register::BH;
    case XED_REG_BL:
        return Register::BL;
    case XED_REG_RCX:
        return Register::RCX;
    case XED_REG_ECX:
        return Register::ECX;
    case XED_REG_CX:
        return Register::CX;
    case XED_REG_CH:
        return Register::CH;
    case XED_REG_CL:
        return Register::CL;
    case XED_REG_RDX:
        return Register::RDX;
    case XED_REG_EDX:
        return Register::EDX;
    case XED_REG_DX:
        return Register::DX;
    case XED_REG_DH:
        return Register::DH;
    case XED_REG_DL:
        return Register::DL;
    case XED_REG_RSI:
        return Register::RSI;
    case XED_REG_ESI:
        return Register::ESI;
    case XED_REG_SI:
        return Register::SI;
    case XED_REG_SIL:
        return Register::SIL;
    case XED_REG_RDI:
        return Register::RDI;
    case XED_REG_EDI:
        return Register::EDI;
    case XED_REG_DI:
        return Register::DI;
    case XED_REG_DIL:
        return Register::DIL;
    case XED_REG_RBP:
        return Register::RBP;
    case XED_REG_EBP:
        return Register::EBP;
    case XED_REG_BP:
        return Register::BP;
    case XED_REG_BPL:
        return Register::BPL;
    case XED_REG_RSP:
        return Register::RSP;
    case XED_REG_ESP:
        return Register::ESP;
    case XED_REG_SP:
        return Register::SP;
    case XED_REG_SPL:
        return Register::SPL;
    case XED_REG_R8:
        return Register::R8;
    case XED_REG_R8D:
        return Register::R8D;
    case XED_REG_R8W:
        return Register::R8W;
    case XED_REG_R8B:
        return Register::R8B;
    case XED_REG_R9:
        return Register::R9;
    case XED_REG_R9D:
        return Register::R9D;
    case XED_REG_R9W:
        return Register::R9W;
    case XED_REG_R9B:
        return Register::R9B;
    case XED_REG_R10:
        return Register::R10;
    case XED_REG_R10D:
        return Register::R10D;
    case XED_REG_R10W:
        return Register::R10W;
    case XED_REG_R10B:
        return Register::R10B;
    case XED_REG_R11:
        return Register::R11;
    case XED_REG_R11D:
        return Register::R11D;
    case XED_REG_R11W:
        return Register::R11W;
    case XED_REG_R11B:
        return Register::R11B;
    case XED_REG_R12:
        return Register::R12;
    case XED_REG_R12D:
        return Register::R12D;
    case XED_REG_R12W:
        return Register::R12W;
    case XED_REG_R12B:
        return Register::R12B;
    case XED_REG_R13:
        return Register::R13;
    case XED_REG_R13D:
        return Register::R13D;
    case XED_REG_R13W:
        return Register::R13W;
    case XED_REG_R13B:
        return Register::R13B;
    case XED_REG_R14:
        return Register::R14;
    case XED_REG_R14D:
        return Register::R14D;
    case XED_REG_R14W:
        return Register::R14W;
    case XED_REG_R14B:
        return Register::R14B;
    case XED_REG_R15:
        return Register::R15;
    case XED_REG_R15D:
        return Register::R15D;
    case XED_REG_R15W:
        return Register::R15W;
    case XED_REG_R15B:
        return Register::R15B;
    case XED_REG_XMM0:
        return Register::XMM0;
    case XED_REG_XMM1:
        return Register::XMM1;
    case XED_REG_XMM2:
        return Register::XMM2;
    case XED_REG_XMM3:
        return Register::XMM3;
    case XED_REG_XMM4:
        return Register::XMM4;
    case XED_REG_XMM5:
        return Register::XMM5;
    case XED_REG_XMM6:
        return Register::XMM6;
    case XED_REG_XMM7:
        return Register::XMM7;
    case XED_REG_XMM8:
        return Register::XMM8;
    case XED_REG_XMM9:
        return Register::XMM9;
    case XED_REG_XMM10:
        return Register::XMM10;
    case XED_REG_XMM11:
        return Register::XMM11;
    case XED_REG_XMM12:
        return Register::XMM12;
    case XED_REG_XMM13:
        return Register::XMM13;
    case XED_REG_XMM14:
        return Register::XMM14;
    case XED_REG_XMM15:
        return Register::XMM15;
    case XED_REG_INVALID:
        return Register::None;
    default:
        drob_assert_not_reached();
    }
}

static void translate_regop(const xed_decoded_inst_t &xedd, int regidx, StaticOperand &op)
{
    switch (regidx) {
    case 0:
        op.reg = translate_reg(xed_decoded_inst_get_reg(&xedd, XED_OPERAND_REG0));
        break;
    case 1:
        op.reg = translate_reg(xed_decoded_inst_get_reg(&xedd, XED_OPERAND_REG1));
        break;
    default:
        drob_assert_not_reached();
    }
}

static void translate_memop(const xed_decoded_inst_t &xedd, int memidx, StaticOperand &op)
{
    if (xed_decoded_inst_get_base_reg(&xedd, memidx) == XED_REG_RIP) {
        /* Convert RIP+rel32 to absolute address */
        op.mem.type = MemPtrType::Direct;
        op.mem.addr.val = (uint64_t)xedd._byte_array._dec +
                          xed_decoded_inst_get_length(&xedd) +
                          (int64_t)xed_decoded_inst_get_memory_displacement(&xedd, memidx);
        op.mem.addr.usrPtrNr = -1;
    } else if (xed_decoded_inst_get_base_reg(&xedd, memidx) == XED_REG_INVALID &&
           xed_decoded_inst_get_index_reg(&xedd, memidx) == XED_REG_INVALID) {
        /* Convert Disp32 only to absolute address */
        /* FIXME: will this directly include moffs for MOV? */
        op.mem.type = MemPtrType::Direct;
        op.mem.addr.val = (uint32_t)xed_decoded_inst_get_memory_displacement(&xedd, memidx);
        op.mem.addr.usrPtrNr = -1;
    } else {
        op.mem.type = MemPtrType::SIB;
        op.mem.sib.base = translate_reg(xed_decoded_inst_get_base_reg(&xedd, memidx));
        op.mem.sib.index = translate_reg(xed_decoded_inst_get_index_reg(&xedd, memidx));
        op.mem.sib.scale = xed_decoded_inst_get_scale(&xedd, memidx);
        op.mem.sib.disp.val = xed_decoded_inst_get_memory_displacement(&xedd, memidx);
        op.mem.sib.disp.usrPtrNr = -1;
    }
}

static void translate_rel(const xed_decoded_inst_t &xedd, StaticOperand &op)
{
    op.mem.type = MemPtrType::Direct;
    op.mem.addr.val = (uint64_t)xedd._byte_array._dec + xed_decoded_inst_get_length(&xedd) +
                       xed_decoded_inst_get_branch_displacement(&xedd);
    op.mem.addr.usrPtrNr = -1;
}

static void translate_imm(const xed_decoded_inst_t &xedd, StaticOperand &op)
{
    op.imm.usrPtrNr = -1;
    if (!xed_decoded_inst_get_immediate_is_signed(&xedd)) {
        op.imm.val = xed_decoded_inst_get_unsigned_immediate(&xedd);
        return;
    }
    op.imm.val = (int64_t)xed_decoded_inst_get_signed_immediate(&xedd);
}

static bool has_imm(const xed_decoded_inst_t &xedd)
{
    return !!xed_decoded_inst_get_immediate_width_bits(&xedd);
}

static bool is_memop(const xed_decoded_inst_t &xedd, int opidx)
{
    const xed_inst_t *xi = xed_decoded_inst_inst(&xedd);
    const xed_operand_t *op = xed_inst_operand(xi, opidx);
    xed_operand_enum_t op_name = xed_operand_name(op);

    return op_name == XED_OPERAND_AGEN ||
           op_name == XED_OPERAND_MEM0 ||
           op_name == XED_OPERAND_MEM1;
}

static void translate_mr(const xed_decoded_inst_t &xedd, ExplicitStaticOperands *operands)
{
    translate_memop(xedd, 0, operands->op[0]);
    translate_regop(xedd, 0, operands->op[1]);
}

static void translate_rm(const xed_decoded_inst_t &xedd, ExplicitStaticOperands *operands)
{
    translate_regop(xedd, 0, operands->op[0]);
    translate_memop(xedd, 0, operands->op[1]);
}

static void translate_rr(const xed_decoded_inst_t &xedd, ExplicitStaticOperands *operands)
{
    translate_regop(xedd, 0, operands->op[0]);
    translate_regop(xedd, 1, operands->op[1]);
}

static void translate_mi(const xed_decoded_inst_t &xedd, ExplicitStaticOperands *operands)
{
    translate_memop(xedd, 0, operands->op[0]);
    translate_imm(xedd, operands->op[1]);
}

static void translate_ri(const xed_decoded_inst_t &xedd, ExplicitStaticOperands *operands)
{
    translate_regop(xedd, 0, operands->op[0]);
    translate_imm(xedd, operands->op[1]);
}

static const OpcodeInfo *convert_rm(const xed_decoded_inst_t &xedd, Opcode *opcode,
                    ExplicitStaticOperands *operands, Opcode rm)
{
    translate_rm(xedd, operands);
    *opcode = rm;
    return getOpcodeInfo(rm);
}

static const OpcodeInfo *convert_rm_rr(const xed_decoded_inst_t &xedd, Opcode *opcode,
                       ExplicitStaticOperands *operands, Opcode rm, Opcode rr)
{
    if (is_memop(xedd, 1)) {
        translate_rm(xedd, operands);
        *opcode = rm;
        return getOpcodeInfo(rm);
    }
    translate_rr(xedd, operands);
    *opcode = rr;
    return getOpcodeInfo(rr);
}

static const OpcodeInfo *convert_m_r(const xed_decoded_inst_t &xedd, Opcode *opcode,
                     ExplicitStaticOperands *operands, Opcode m, Opcode r)
{
    if (is_memop(xedd, 0)) {
        translate_memop(xedd, 0, operands->op[0]);
        *opcode = m;
        return getOpcodeInfo(m);
    }
    translate_regop(xedd, 0, operands->op[0]);
    *opcode = r;
    return getOpcodeInfo(r);
}

static const OpcodeInfo *convert_m_r_i(const xed_decoded_inst_t &xedd, Opcode *opcode,
                       ExplicitStaticOperands *operands, Opcode m, Opcode r,
                       Opcode s)
{
    if (is_memop(xedd, 0)) {
        translate_memop(xedd, 0, operands->op[0]);
        *opcode = m;
        return getOpcodeInfo(m);
    } else if (has_imm(xedd)) {
        translate_imm(xedd, operands->op[0]);
        *opcode = s;
        return getOpcodeInfo(s);
    }
    translate_regop(xedd, 0, operands->op[0]);
    *opcode = r;
    return getOpcodeInfo(r);
}

static const OpcodeInfo *convert_mr_rm_rr(const xed_decoded_inst_t &xedd,
                      Opcode *opcode, ExplicitStaticOperands *operands,
                      Opcode mr, Opcode rm, Opcode rr)
{
    if (is_memop(xedd, 1)) {
        translate_rm(xedd, operands);
        *opcode = rm;
        return getOpcodeInfo(rm);
    } else if (is_memop(xedd, 0)) {
        translate_mr(xedd, operands);
        *opcode = mr;
        return getOpcodeInfo(mr);
    }
    translate_rr(xedd, operands);
    *opcode = rr;
    return getOpcodeInfo(rr);
}

static const OpcodeInfo *convert_mr_mi_rr_ri(const xed_decoded_inst_t &xedd,
                         Opcode *opcode, ExplicitStaticOperands *operands,
                         Opcode mr, Opcode ms, Opcode rr, Opcode rs)
{
    if (is_memop(xedd, 0)) {
        if (has_imm(xedd)) {
            translate_mi(xedd, operands);
            *opcode = ms;
            return getOpcodeInfo(ms);
        }
        translate_mr(xedd, operands);
        *opcode = mr;
        return getOpcodeInfo(mr);
    } else if (has_imm(xedd)) {
        translate_ri(xedd, operands);
        *opcode = rs;
        return getOpcodeInfo(rs);
    }
    translate_rr(xedd, operands);
    *opcode = rr;
    return getOpcodeInfo(rr);
}

static const OpcodeInfo *convert_mr_mi_rm_rr_ri(const xed_decoded_inst_t &xedd,
                        Opcode *opcode, ExplicitStaticOperands *operands,
                        Opcode mr, Opcode ms, Opcode rm,
                        Opcode rr, Opcode rs)
{
    if (is_memop(xedd, 0)) {
        if (has_imm(xedd)) {
            translate_mi(xedd, operands);
            *opcode = ms;
            return getOpcodeInfo(ms);
        }
        translate_mr(xedd, operands);
        *opcode = mr;
        return getOpcodeInfo(mr);
    } else if (is_memop(xedd, 1)) {
        translate_rm(xedd, operands);
        *opcode = rm;
        return getOpcodeInfo(rm);
    } else if (has_imm(xedd)) {
        translate_ri(xedd, operands);
        *opcode = rs;
        return getOpcodeInfo(rs);
    }
    translate_rr(xedd, operands);
    *opcode = rr;
    return getOpcodeInfo(rr);
}

static const OpcodeInfo *convert_add(const xed_decoded_inst_t &xedd,
                       Opcode *opcode, ExplicitStaticOperands *operands)
{
    switch(xed_decoded_inst_get_operand_width(&xedd)) {
    case 8:
        return convert_mr_mi_rm_rr_ri(xedd, opcode, operands, Opcode::ADD8mr,
                                      Opcode::ADD8mi, Opcode::ADD8rm,
                                      Opcode::ADD8rr, Opcode::ADD8ri);
    case 16:
        return convert_mr_mi_rm_rr_ri(xedd, opcode, operands, Opcode::ADD16mr,
                                      Opcode::ADD16mi, Opcode::ADD16rm,
                                      Opcode::ADD16rr, Opcode::ADD16ri);
    case 32:
        return convert_mr_mi_rm_rr_ri(xedd, opcode, operands, Opcode::ADD32mr,
                                      Opcode::ADD32mi, Opcode::ADD32rm,
                                      Opcode::ADD32rr, Opcode::ADD32ri);
    case 64:
        return convert_mr_mi_rm_rr_ri(xedd, opcode, operands, Opcode::ADD64mr,
                                      Opcode::ADD64mi, Opcode::ADD64rm,
                                      Opcode::ADD64rr, Opcode::ADD64ri);
    default:
        return nullptr;
    }
}

static const OpcodeInfo *convert_addpd(const xed_decoded_inst_t &xedd,
                       Opcode *opcode, ExplicitStaticOperands *operands)
{
    return convert_rm_rr(xedd, opcode, operands, Opcode::ADDPDrm, Opcode::ADDPDrr);
}

static const OpcodeInfo *convert_addsd(const xed_decoded_inst_t &xedd,
                       Opcode *opcode, ExplicitStaticOperands *operands)
{
    return convert_rm_rr(xedd, opcode, operands, Opcode::ADDSDrm, Opcode::ADDSDrr);
}

static const OpcodeInfo *convert_call(const xed_decoded_inst_t &xedd,
                      Opcode *opcode, ExplicitStaticOperands *operands)
{
    if (xed_operand_values_has_address_size_prefix(&xedd)) {
        return nullptr;
    }
    if (xed_decoded_inst_get_branch_displacement_width(&xedd)) {
        translate_rel(xedd, operands->op[0]);
        RETURN_OPCODE(CALLa);
    }
    return convert_m_r(xedd, opcode, operands, Opcode::CALLm, Opcode::CALLr);
}

static const OpcodeInfo *convert_cmp(const xed_decoded_inst_t &xedd,
                                     Opcode *opcode, ExplicitStaticOperands *operands)
{
    switch (xed_decoded_inst_get_operand_width(&xedd)) {
    case 8:
        return convert_mr_mi_rm_rr_ri(xedd, opcode, operands, Opcode::CMP8mr,
                                      Opcode::CMP8mi, Opcode::CMP8rm,
                                      Opcode::CMP8rr, Opcode::CMP8ri);
    case 16:
        return convert_mr_mi_rm_rr_ri(xedd, opcode, operands, Opcode::CMP16mr,
                                      Opcode::CMP16mi, Opcode::CMP16rm,
                                      Opcode::CMP16rr, Opcode::CMP16ri);
    case 32:
        return convert_mr_mi_rm_rr_ri(xedd, opcode, operands, Opcode::CMP32mr,
                                      Opcode::CMP32mi, Opcode::CMP32rm,
                                      Opcode::CMP32rr, Opcode::CMP32ri);
    case 64:
        return convert_mr_mi_rm_rr_ri(xedd, opcode, operands, Opcode::CMP64mr,
                                      Opcode::CMP64mi, Opcode::CMP64rm,
                                      Opcode::CMP64rr, Opcode::CMP64ri);
    default:
        drob_assert_not_reached();
    }
}

static const OpcodeInfo *convert_jcc(const xed_decoded_inst_t &xedd,
                                     Opcode *opcode, ExplicitStaticOperands *operands)
{
    if (unlikely(xed_operand_values_has_address_size_prefix(&xedd))) {
        translate_rel(xedd, operands->op[0]);
        if (xed_decoded_inst_get_iclass(&xedd) == XED_ICLASS_JCXZ) {
            RETURN_OPCODE(JCXZ64a);
        }
        return nullptr;
    }
    translate_rel(xedd, operands->op[0]);
    switch (xed_decoded_inst_get_iclass(&xedd)) {
    case XED_ICLASS_JNBE:
        RETURN_OPCODE(JNBEa);
    case XED_ICLASS_JNB:
        RETURN_OPCODE(JNBa);
    case XED_ICLASS_JB:
        RETURN_OPCODE(JBa);
    case XED_ICLASS_JBE:
        RETURN_OPCODE(JBEa);
    case XED_ICLASS_JCXZ:
        RETURN_OPCODE(JCXZ32a);
    case XED_ICLASS_JZ:
        RETURN_OPCODE(JZa);
    case XED_ICLASS_JNLE:
        RETURN_OPCODE(JNLEa);
    case XED_ICLASS_JNL:
        RETURN_OPCODE(JNLa);
    case XED_ICLASS_JL:
        RETURN_OPCODE(JLa);
    case XED_ICLASS_JLE:
        RETURN_OPCODE(JLEa);
    case XED_ICLASS_JNZ:
        RETURN_OPCODE(JNZa);
    case XED_ICLASS_JNO:
        RETURN_OPCODE(JNOa);
    case XED_ICLASS_JNP:
        RETURN_OPCODE(JNPa);
    case XED_ICLASS_JNS:
        RETURN_OPCODE(JNSa);
    case XED_ICLASS_JO:
        RETURN_OPCODE(JOa);
    case XED_ICLASS_JP:
        RETURN_OPCODE(JPa);
    case XED_ICLASS_JS:
        RETURN_OPCODE(JSa);
    default:
        drob_assert_not_reached();
    }
}

static const OpcodeInfo *convert_jmp(const xed_decoded_inst_t &xedd,
                     Opcode *opcode, ExplicitStaticOperands *operands)
{
    if (xed_operand_values_has_address_size_prefix(&xedd)) {
        drob_throw("0x67 prefix with jumps not supported");
    }
    if (xed_decoded_inst_get_branch_displacement_width(&xedd)) {
        translate_rel(xedd, operands->op[0]);
        RETURN_OPCODE(JMPa);
    }
    return convert_m_r(xedd, opcode, operands, Opcode::JMPm, Opcode::JMPr);
}

static const OpcodeInfo *convert_lea(const xed_decoded_inst_t &xedd,
                       Opcode *opcode, ExplicitStaticOperands *operands)
{
    switch (xed_decoded_inst_get_operand_width(&xedd)) {
    case 16:
        return convert_rm(xedd, opcode, operands, Opcode::LEA16ra);
    case 32:
        return convert_rm(xedd, opcode, operands, Opcode::LEA32ra);
    case 64:
        return convert_rm(xedd, opcode, operands, Opcode::LEA64ra);
    default:
        drob_assert_not_reached();
    }
}

static const OpcodeInfo *convert_mov(const xed_decoded_inst_t &xedd,
                     Opcode *opcode, ExplicitStaticOperands *operands)
{
    switch (xed_decoded_inst_get_operand_width(&xedd)) {
    case 32:
        return convert_mr_mi_rm_rr_ri(xedd, opcode, operands, Opcode::MOV32mr,
                          Opcode::MOV32mi, Opcode::MOV32rm,
                          Opcode::MOV32rr, Opcode::MOV32ri);
    case 64:
        return convert_mr_mi_rm_rr_ri(xedd, opcode, operands, Opcode::MOV64mr,
                          Opcode::MOV64mi, Opcode::MOV64rm,
                          Opcode::MOV64rr, Opcode::MOV64ri);
    default:
        drob_assert_not_reached();
    }
}

static const OpcodeInfo *convert_movapd(const xed_decoded_inst_t &xedd,
                    Opcode *opcode, ExplicitStaticOperands *operands)
{
    return convert_mr_rm_rr(xedd, opcode, operands, Opcode::MOVAPDmr,
                Opcode::MOVAPDrm, Opcode::MOVAPDrr);
}

static const OpcodeInfo *convert_movsd(const xed_decoded_inst_t &xedd,
                       Opcode *opcode, ExplicitStaticOperands *operands)
{
    return convert_mr_rm_rr(xedd, opcode, operands, Opcode::MOVSDmr,
                Opcode::MOVSDrm, Opcode::MOVSDrr);
}

static const OpcodeInfo *convert_movupd(const xed_decoded_inst_t &xedd,
                    Opcode *opcode, ExplicitStaticOperands *operands)
{
    return convert_mr_rm_rr(xedd, opcode, operands, Opcode::MOVUPDmr,
                Opcode::MOVUPDrm, Opcode::MOVUPDrr);
}

static const OpcodeInfo *convert_movups(const xed_decoded_inst_t &xedd,
                    Opcode *opcode, ExplicitStaticOperands *operands)
{
    return convert_mr_rm_rr(xedd, opcode, operands, Opcode::MOVUPSmr,
                Opcode::MOVUPSrm, Opcode::MOVUPSrr);
}

static const OpcodeInfo *convert_mulpd(const xed_decoded_inst_t &xedd,
                       Opcode *opcode, ExplicitStaticOperands *operands)
{
    return convert_rm_rr(xedd, opcode, operands, Opcode::MULPDrm,
                 Opcode::MULPDrr);
}

static const OpcodeInfo *convert_mulsd(const xed_decoded_inst_t &xedd,
                       Opcode *opcode, ExplicitStaticOperands *operands)
{
    return convert_rm_rr(xedd, opcode, operands, Opcode::MULSDrm,
                 Opcode::MULSDrr);
}

static const OpcodeInfo *convert_pop(const xed_decoded_inst_t &xedd,
                     Opcode *opcode, ExplicitStaticOperands *operands)
{
    switch (xed_decoded_inst_get_operand_width(&xedd)) {
    case 16:
        return convert_m_r(xedd, opcode, operands, Opcode::POP16m,
                   Opcode::POP16r);
    case 64:
        return convert_m_r(xedd, opcode, operands, Opcode::POP64m,
                   Opcode::POP64r);
    default:
        drob_assert_not_reached();
    }
}

static const OpcodeInfo *convert_push(const xed_decoded_inst_t &xedd,
                      Opcode *opcode, ExplicitStaticOperands *operands)
{
    switch (xed_decoded_inst_get_operand_width(&xedd)) {
    case 16:
        return convert_m_r_i(xedd, opcode, operands, Opcode::PUSH16m,
                     Opcode::PUSH16r, Opcode::PUSH16i);
    case 64:
        return convert_m_r_i(xedd, opcode, operands, Opcode::PUSH64m,
                     Opcode::PUSH64r, Opcode::PUSH64i);
    default:
        drob_assert_not_reached();
    }
}

static const OpcodeInfo *convert_pxor(const xed_decoded_inst_t &xedd,
                      Opcode *opcode, ExplicitStaticOperands *operands)
{
    if (xed_decoded_inst_get_extension(&xedd) != XED_EXTENSION_SSE2) {
        return nullptr;
    }
    return convert_rm_rr(xedd, opcode, operands, Opcode::PXOR128rm, Opcode::PXOR128rr);
}

static const OpcodeInfo *convert_ret(const xed_decoded_inst_t &xedd,
                     Opcode *opcode, ExplicitStaticOperands *operands)
{
    if (xed_operand_values_has_address_size_prefix(&xedd)) {
        return nullptr;
    }
    if (xed_decoded_inst_get_unsigned_immediate(&xedd)) {
        return nullptr;
    }
    (void)operands;
    RETURN_OPCODE(RET);
}

static const OpcodeInfo *convert_shl(const xed_decoded_inst_t &xedd,
                     Opcode *opcode, ExplicitStaticOperands *operands)
{
    if (xed_decoded_inst_get_operand_width(&xedd) != 64) {
        return nullptr;
    }
    if (is_memop(xedd, 0)) {
        if (has_imm(xedd)) {
            translate_mi(xedd, operands);
            RETURN_OPCODE(SHL64mi);
        }
        translate_memop(xedd, 0, operands->op[0]);
        RETURN_OPCODE(SHL64m);
    }
    if (has_imm(xedd)) {
        translate_ri(xedd, operands);
        RETURN_OPCODE(SHL64ri);
    }
    translate_regop(xedd, 0, operands->op[0]);
    RETURN_OPCODE(SHL64r);
}

static const OpcodeInfo *convert_shr(const xed_decoded_inst_t &xedd,
                                     Opcode *opcode,
                                     ExplicitStaticOperands *operands)
{
    if (xed_decoded_inst_get_operand_width(&xedd) != 64) {
        return nullptr;
    }
    if (is_memop(xedd, 0)) {
        if (has_imm(xedd)) {
            translate_mi(xedd, operands);
            RETURN_OPCODE(SHR64mi);
        }
        translate_memop(xedd, 0, operands->op[0]);
        RETURN_OPCODE(SHR64m);
    }
    if (has_imm(xedd)) {
        translate_ri(xedd, operands);
        RETURN_OPCODE(SHR64ri);
    }
    translate_regop(xedd, 0, operands->op[0]);
    RETURN_OPCODE(SHR64r);
}

static const OpcodeInfo *convert_sub(const xed_decoded_inst_t &xedd,
                                     Opcode *opcode, ExplicitStaticOperands *operands)
{
    switch(xed_decoded_inst_get_operand_width(&xedd)) {
    case 8:
        return convert_mr_mi_rm_rr_ri(xedd, opcode, operands, Opcode::SUB8mr,
                                      Opcode::SUB8mi, Opcode::SUB8rm,
                                      Opcode::SUB8rr, Opcode::SUB8ri);
    case 16:
        return convert_mr_mi_rm_rr_ri(xedd, opcode, operands, Opcode::SUB16mr,
                                      Opcode::SUB16mi, Opcode::SUB16rm,
                                      Opcode::SUB16rr, Opcode::SUB16ri);
    case 32:
        return convert_mr_mi_rm_rr_ri(xedd, opcode, operands, Opcode::SUB32mr,
                                      Opcode::SUB32mi, Opcode::SUB32rm,
                                      Opcode::SUB32rr, Opcode::SUB32ri);
    case 64:
        return convert_mr_mi_rm_rr_ri(xedd, opcode, operands, Opcode::SUB64mr,
                                      Opcode::SUB64mi, Opcode::SUB64rm,
                                      Opcode::SUB64rr, Opcode::SUB64ri);
    default:
        return nullptr;
    }
}

static const OpcodeInfo *convert_test(const xed_decoded_inst_t &xedd,
                      Opcode *opcode, ExplicitStaticOperands *operands)
{
    switch (xed_decoded_inst_get_operand_width(&xedd)) {
    case 8:
        return convert_mr_mi_rr_ri(xedd, opcode, operands, Opcode::TEST8mr,
                       Opcode::TEST8mi, Opcode::TEST8rr,
                       Opcode::TEST8ri);
    case 16:
        return convert_mr_mi_rr_ri(xedd, opcode, operands, Opcode::TEST16mr,
                       Opcode::TEST16mi, Opcode::TEST16rr,
                       Opcode::TEST16ri);
    case 32:
        return convert_mr_mi_rr_ri(xedd, opcode, operands, Opcode::TEST32mr,
                       Opcode::TEST32mi, Opcode::TEST32rr,
                       Opcode::TEST32ri);
    case 64:
        return convert_mr_mi_rr_ri(xedd, opcode, operands, Opcode::TEST64mr,
                       Opcode::TEST64mi, Opcode::TEST64rr,
                       Opcode::TEST64ri);
    default:
        drob_assert_not_reached();
    }
}

static const OpcodeInfo *convert_xor(const xed_decoded_inst_t &xedd,
                       Opcode *opcode, ExplicitStaticOperands *operands)
{
    switch (xed_decoded_inst_get_operand_width(&xedd)) {
    case 32:
        return convert_mr_mi_rm_rr_ri(xedd, opcode, operands, Opcode::XOR32mr,
                          Opcode::XOR32mi, Opcode::XOR32rm,
                          Opcode::XOR32rr, Opcode::XOR32ri);
    case 64:
        return convert_mr_mi_rm_rr_ri(xedd, opcode, operands, Opcode::XOR64mr,
                          Opcode::XOR64mi, Opcode::XOR64rm,
                          Opcode::XOR64rr, Opcode::XOR64ri);
    default:
        return nullptr;
    }
}

static const OpcodeInfo *convert_simple(const xed_decoded_inst_t &xedd,
                                        Opcode *opcode, ExplicitStaticOperands *operands)
{
    switch (xed_decoded_inst_get_iclass(&xedd)) {
    case XED_ICLASS_ADD:
        return convert_add(xedd, opcode, operands);
    case XED_ICLASS_ADDPD:
        return convert_addpd(xedd, opcode, operands);
    case XED_ICLASS_ADDSD:
        return convert_addsd(xedd, opcode, operands);
    case XED_ICLASS_CALL_NEAR:
        return convert_call(xedd, opcode, operands);
    case XED_ICLASS_CMP:
        return convert_cmp(xedd, opcode, operands);
    case XED_ICLASS_JNBE:
    case XED_ICLASS_JNB:
    case XED_ICLASS_JB:
    case XED_ICLASS_JBE:
    case XED_ICLASS_JCXZ:
    case XED_ICLASS_JZ:
    case XED_ICLASS_JNLE:
    case XED_ICLASS_JNL:
    case XED_ICLASS_JL:
    case XED_ICLASS_JLE:
    case XED_ICLASS_JNZ:
    case XED_ICLASS_JNO:
    case XED_ICLASS_JNP:
    case XED_ICLASS_JNS:
    case XED_ICLASS_JO:
    case XED_ICLASS_JP:
    case XED_ICLASS_JS:
        return convert_jcc(xedd, opcode, operands);
    case XED_ICLASS_JMP:
        return convert_jmp(xedd, opcode, operands);
    case XED_ICLASS_LEA:
        return convert_lea(xedd, opcode, operands);
    case XED_ICLASS_MOV:
        return convert_mov(xedd, opcode, operands);
    case XED_ICLASS_MOVAPD:
        return convert_movapd(xedd, opcode, operands);
    case XED_ICLASS_MOVSD_XMM:
        return convert_movsd(xedd, opcode, operands);
    case XED_ICLASS_MOVUPD:
        return convert_movupd(xedd, opcode, operands);
    case XED_ICLASS_MOVUPS:
        return convert_movups(xedd, opcode, operands);
    case XED_ICLASS_MULPD:
        return convert_mulpd(xedd, opcode, operands);
    case XED_ICLASS_MULSD:
        return convert_mulsd(xedd, opcode, operands);
    case XED_ICLASS_POP:
        return convert_pop(xedd, opcode, operands);
    case XED_ICLASS_PUSH:
        return convert_push(xedd, opcode, operands);
    case XED_ICLASS_PXOR:
        return convert_pxor(xedd, opcode, operands);
    case XED_ICLASS_RET_NEAR:
        return convert_ret(xedd, opcode, operands);
    case XED_ICLASS_SHL:
        return convert_shl(xedd, opcode, operands);
    case XED_ICLASS_SHR:
        return convert_shr(xedd, opcode, operands);
    case XED_ICLASS_SUB:
        return convert_sub(xedd, opcode, operands);
    case XED_ICLASS_TEST:
        return convert_test(xedd, opcode, operands);
    case XED_ICLASS_XOR:
        return convert_xor(xedd, opcode, operands);
    default:
        return nullptr;
    };
}

static bool is_rip_relative(const xed_decoded_inst_t &xedd)
{
    int memops = xed_decoded_inst_number_of_memory_operands(&xedd);
    xed_reg_enum_t base;

    /*
     * Unknown instructions that use RIP-relative instructions cannot
     * be relocated.
     */
    for(int i = 0; i < memops; i++) {
        base = xed_decoded_inst_get_base_reg(&xedd, i);
        if (base == XED_REG_RIP || base == XED_REG_EIP) {
            return true;
        }
    }
    return false;
}

/*
 * LOOP is ugly because it is a conditional jump, but action is performed before
 * the actual predicate is checked. Even worse, the predicates can't be modeled
 * directly via JMPs, as these variants don't exist. So we need multiple JMPs.
 *
 * We will convert LOOP into a sequence of instructions that will keep the
 * exact same behavior (esp. not modify unrelated registers). This allows to
 * model it in drob and eventually optimize it perfectly when unrolling loops.
 *
 * LOOP:
 *      LEA %RCX, [%RCX - 1]    // Will not modify EFLAGS!
 *      JCXZ64 %NEXT_INSTR_ADDR // If %RCX == 0 -> stop loop
 *      JMP %ORIG_JMP_ADDR      // If %RCX != 0 -> continue loop
 *
 * LOOPE:
 *      LEA %RCX, [%RCX - 1]
 *      JCXZ64 %NEXT_INSTR_ADDR
 *      JNZ %NEXT_INSTR_ADDR    // ZF flag from other instructions is relevant!
 *      JMP %ORIG_JMP_ADDR      // If %RCX != 0 AND ZF == 1 -> continue loop
 * Similar for LOOPNE.
 *
 * The loop itself might execute slower. The loop is still one block in the
 * best case. But we don't optimize for LOOPs, just support them in some
 * way.
 */
static DecodeRet convert_loop(const xed_decoded_inst_t &xedd,
                              std::list<std::unique_ptr<Instruction>> &instrs)
{
    const uint8_t ilen = (uint8_t)xed_decoded_inst_get_length(&xedd);
    const bool is64bit = xed_operand_values_has_address_size_prefix(&xedd);
    const bool isLOOPE = xed_decoded_inst_get_iclass(&xedd) == XED_ICLASS_LOOPE;
    const bool isLOOPNE = xed_decoded_inst_get_iclass(&xedd) == XED_ICLASS_LOOPNE;
    ExplicitStaticOperands operands = {};
    Opcode opcode;

    /* LEA to do the subtraction */
    opcode = is64bit ? Opcode::LEA64ra : Opcode::LEA32ra;
    operands.op[0].reg = is64bit ? Register::RCX : Register::ECX;
    operands.op[1].mem.type = MemPtrType::SIB;
    operands.op[1].mem.sib.base = operands.op[0].reg;
    operands.op[1].mem.sib.index = Register::None;
    operands.op[1].mem.sib.disp.val = -1;
    operands.op[1].mem.sib.disp.usrPtrNr = -1;
    instrs.emplace_back(std::make_unique<Instruction>(xedd._byte_array._dec,
                                                      ilen, opcode, operands,
                                                      getOpcodeInfo(opcode),
                                                      true));

    /* JCXZ to the next instruction */
    opcode = is64bit ? Opcode::JCXZ64a : Opcode::JCXZ32a;
    operands = {};
    operands.op[0].mem.type = MemPtrType::Direct;
    operands.op[0].mem.addr.val = (uint64_t)xedd._byte_array._dec + ilen;
    operands.op[0].mem.addr.usrPtrNr = -1;
    instrs.emplace_back(std::make_unique<Instruction>(xedd._byte_array._dec,
                                                      ilen, opcode, operands,
                                                      getOpcodeInfo(opcode),
                                                      true));

    /* JNZ/JZ to the next instruction */
    if (isLOOPE || isLOOPNE) {
        opcode = isLOOPE ? Opcode::JNZa : Opcode::JZa;
        /* reuse operand from JCXZ */
        instrs.emplace_back(std::make_unique<Instruction>(xedd._byte_array._dec,
                                                          ilen, opcode, operands,
                                                          getOpcodeInfo(opcode),
                                                          true));
    }

    /* final JMPa to the original loop target */
    opcode = Opcode::JMPa;
    operands = {};
    translate_rel(xedd, operands.op[0]);
    instrs.emplace_back(std::make_unique<Instruction>(xedd._byte_array._dec,
                                                      ilen, opcode, operands,
                                                      getOpcodeInfo(opcode),
                                                      true));
    return DecodeRet::EOB;
}

DecodeRet convert_decoded(const xed_decoded_inst_t &xedd,
                          std::list<std::unique_ptr<Instruction>> &instrs,
                          const RewriterCfg &cfg)
{
    const uint8_t ilen = (uint8_t)xed_decoded_inst_get_length(&xedd);

    switch (xed_decoded_inst_get_iclass(&xedd)) {
    case XED_ICLASS_LOOP:
    case XED_ICLASS_LOOPE:
    case XED_ICLASS_LOOPNE:
        return convert_loop(xedd, instrs);
    default: {
        bool reencode = is_rip_relative(xedd);
        const OpcodeInfo *opcodeInfo;
        ExplicitStaticOperands operands;
        Opcode opcode;

        opcodeInfo = convert_simple(xedd, &opcode, &operands);
        if (!opcodeInfo) {
            if (cfg.getDrobCfg().fail_on_unmodelled) {
                arch_decode_dump(xedd._byte_array._dec, xedd._byte_array._dec + ilen);
                drob_throw("Unmodelled instruction detected");
            }
            if (reencode) {
                drob_throw("RIP-relative addressing is not supported"
                           " for unmodelled instructions");
            }
            opcode = Opcode::NONE;
        }
        instrs.emplace_back(std::make_unique<Instruction>(xedd._byte_array._dec,
                                                          ilen, opcode, operands,
                                                          opcodeInfo, reencode));

        switch (xed_decoded_inst_get_category(&xedd)) {
        case XED_CATEGORY_CALL:
        case XED_CATEGORY_UNCOND_BR:
        case XED_CATEGORY_RET:
            if (!opcodeInfo) {
                drob_throw("Unhandled control flow instruction.");
            }
            return DecodeRet::EOB;
        case XED_CATEGORY_COND_BR:
            if (!opcodeInfo) {
                drob_throw("Unhandled control flow instruction.");
            }
            return DecodeRet::Ok;
        default:
            return DecodeRet::Ok;
        }
    }
    }
}


} /* namespace drob */
