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
#include "InstructionInfo.hpp"
#include "arch.hpp"

namespace drob {

void dump(const Data &data)
{
    switch (data.getType()) {
    case DynamicValueType::Dead:
        drob_dump("      Dead");
        break;
    case DynamicValueType::Unknown:
        drob_dump("      Unknown");
        break;
    case DynamicValueType::Tainted:
        drob_dump("      Tainted");
        break;
    case DynamicValueType::Immediate:
        if (data.isImm64()) {
            drob_dump("      Immediate64: 0x%" PRIx64, data.getImm64());
        } else {
            drob_dump("      Immediate128: 0x%" PRIx64 "-0x%" PRIx64,
                      (uint64_t)(data.getImm128() >> 64), data.getImm128() >> 64);
        }
        break;
    case DynamicValueType::StackPtr:
        if (data.getPtrOffset() >= 0) {
            drob_dump("      StackPtr(%d) + %" PRIi64, data.getNr(), data.getPtrOffset());
        } else {
            drob_dump("      StackPtr(%d) - %" PRIi64, data.getNr(), -data.getPtrOffset());
        }
        break;
    case DynamicValueType::ReturnPtr:
        if (data.getPtrOffset() >= 0) {
            drob_dump("      ReturnPtr(%d) + %" PRIi64, data.getNr(), data.getPtrOffset());
        } else {
            drob_dump("      ReturnPtr(%d) - %" PRIi64, data.getNr(), -data.getPtrOffset());
        }
        break;
    case DynamicValueType::UsrPtr:
        if (data.getPtrOffset() >= 0) {
            drob_dump("      UsrPtr(%d) + %" PRIi64, data.getNr(), data.getPtrOffset());
        } else {
            drob_dump("      UsrPtr(%d) + %" PRIi64, data.getNr(), -data.getPtrOffset());
        }
        break;
    default:
        drob_assert_not_reached();
    }
}

static const char *toStr(AccessMode mode)
{
    switch (mode) {
    case AccessMode::None:
        return "None";
    case AccessMode::Address:
        return "Address";
    case AccessMode::Read:
        return "Read";
    case AccessMode::MayRead:
        return "MayRead";
    case AccessMode::Write:
        return "Write";
    case AccessMode::MayWrite:
        return "MayWrite";
    case AccessMode::ReadWrite:
        return "ReadWrite";
    case AccessMode::MayReadWrite:
        return "MayReadWrite";
    default:
        drob_assert_not_reached();
    }
}

static const char *toStr(RegisterAccessType type)
{
    switch (type) {
    case RegisterAccessType::None:
        return "None";
    case RegisterAccessType::FullZeroParent:
        return "Parent";
    case RegisterAccessType::Full:
        return "Full";
    case RegisterAccessType::H0:
        return "H0";
    case RegisterAccessType::H1:
        return "H1";
    case RegisterAccessType::F0:
        return "F0";
    case RegisterAccessType::F1:
        return "F1";
    case RegisterAccessType::F2:
        return "F2";
    case RegisterAccessType::F3:
        return "F3";
    default:
        drob_assert_not_reached();
    }
}

void dump(const DynamicOperandInfo &info)
{
    drob_dump(" %s Operand(%d)", info.isImpl ? "Implicit" : "Explicit", info.nr);

    switch (info.type) {
    case OperandType::Register:
        drob_dump("    Register: %s, Access: %s, ReadAccess: %s, WriteAccess: %s",
                  arch_get_register_info(info.regAcc.reg)->name,
                  toStr(info.regAcc.mode), toStr(info.regAcc.r),
                  toStr(info.regAcc.w));
        drob_dump("     Input:");
        dump(info.input);
        drob_dump("     Output:");
        dump(info.output);
        break;
    case OperandType::Immediate8:
        drob_dump("    Immediate8: %" PRIx8, (uint8_t)info.input.getImm64());
        break;
    case OperandType::Immediate16:
        drob_dump("    Immediate16: %" PRIx16, (uint16_t)info.input.getImm64());
        break;
    case OperandType::Immediate32:
        drob_dump("    Immediate32: %" PRIx32, (uint32_t)info.input.getImm64());
        break;
    case OperandType::Immediate64:
        drob_dump("    Immediate64: %" PRIx64, (uint64_t)info.input.getImm64());
        break;
    case OperandType::SignedImmediate8:
        drob_dump("    SignedImmediate8: %" PRIi8, (int8_t)info.input.getImm64());
        break;
    case OperandType::SignedImmediate16:
        drob_dump("    SignedImmediate16: %" PRIi16, (int16_t)info.input.getImm64());
        break;
    case OperandType::SignedImmediate32:
        drob_dump("    SignedImmediate32: %" PRIi32, (int32_t)info.input.getImm64());
        break;
    case OperandType::SignedImmediate64:
        drob_dump("    SignedImmediate64: %" PRIi64, (int64_t)info.input.getImm64());
        break;
    case OperandType::MemPtr:
        drob_dump("    Memory Access: %s, Size: %d", toStr(info.memAcc.mode),
                  static_cast<uint8_t>(info.memAcc.size));
        drob_dump("     Ptr:");
        dump(info.memAcc.ptrVal);
        drob_dump("     Input:");
        dump(info.input);
        drob_dump("     Output:");
        dump(info.output);
        break;
    default:
        drob_assert_not_reached();
    }
}

void dump(const DynamicInstructionInfo &info)
{
    if (info.willExecute == TriState::True) {
        drob_dump(" Will execute!");
    } else if (info.willExecute == TriState::False) {
        drob_dump(" Will not execute!");
    } else if (info.willExecute == TriState::Unknown) {
        drob_dump(" Unknown if it will execute!");
    }

    for (const auto& op : info.operands) {
        dump(op);
    }
}

} /* namespace drob */
