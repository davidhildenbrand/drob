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
#ifndef REGISTER_INFO_HPP
#define REGISTER_INFO_HPP

#include "Utils.hpp"
#include "arch_def.h"

extern "C" {
#include "util/bitmap.h"
}

namespace drob {

/*
 * The different register types we know off (and which an opcode can expect).
 */
typedef enum class RegisterType : uint8_t {
    Flag1 = 0,
    Gprs8,
    Gprs16,
    Gprs32,
    Gprs64,
    Sse128,
} RegisterType;

/*
 * 1. Some instructions (esp. vector) only access some parts of a register
 *    or sometimes even read/write different parts.
 * 2. On x86, when storing to a 32bit register, the upper 32bit of the
 *    64bit GPRS are set to zero. For vector registers it depends on the
 *    instruction.
 * 3. In the future, it might make sense to make this dependable on the
 *    actual instruction / parameters. E.g. AND16ri AX, 0xf0 would effectively
 *    only read the upper bytes and not the lower. We need an arch callback
 *    for that (and a type like "Variable").
 */
typedef enum class RegisterAccessType : uint8_t {
    /* Register will not be accessed (dummy value) */
    None = 0,
    /*
     * The full register will be accessed. In addition, on writes, the parent
     * register is set to zero first.
     */
    FullZeroParent,
    /* The full register will be accessed */
    Full,
    /* (Lower/Upper) Halfs are accessed */
    H0,
    H1,
    /* Fourths are accessed */
    F0,
    F1,
    F2,
    F3,
} RegisterAccessType;

#define SUBREGISTER_BITS static_cast<uint16_t>(SubRegister::MAX)

/*
 * Mask of SubRegisters.
 */
typedef struct SubRegisterMask {
    BITMAP_DECLARE(m, SUBREGISTER_BITS);

    void zero(void)
    {
        bitmap_zero(m, SUBREGISTER_BITS);
    }
    void fill(void)
    {
        bitmap_fill(m, SUBREGISTER_BITS);
    }
    SubRegisterMask& operator +=(const SubRegisterMask &rhs)
    {
        bitmap_or(m, m, rhs.m, SUBREGISTER_BITS);
        return *this;
    }
    SubRegisterMask& operator -=(const SubRegisterMask &rhs)
    {
        bitmap_andnot(m, m, rhs.m, SUBREGISTER_BITS);
        return *this;
    }
    bool operator ==(const SubRegisterMask& rhs) const
    {
        return bitmap_equal(m, rhs.m, SUBREGISTER_BITS);
    }
    bool operator !=(const SubRegisterMask& rhs) const
    {
        return !(*this == rhs);
    }
    bool operator !(void) const
    {
        return bitmap_empty(m, SUBREGISTER_BITS);
    }
    void dump(void) const;
} SubRegisterMask;

static inline SubRegisterMask operator&(const SubRegisterMask& lhs,
                                        const SubRegisterMask& rhs)
{
    SubRegisterMask tmp;

    bitmap_and(tmp.m, lhs.m, rhs.m, SUBREGISTER_BITS);
    return tmp;
}

/*
 * Information about one register.
 */
typedef struct RegisterInfo {
    const RegisterType type;
    const char *name;
    /*
     * If this register has no parent: Assigned register number
     * within this type.
     * If this register has a parent: Assigned register number of the
     * parent.
     */
    uint8_t nr;
    /*
     * If this register has no parent: Has to be 0.
     * If this register has a parent: Byte offset within the parent
     * register.
     */
    uint8_t byteOffs;
    /*
     * The outermost parent register, if any. E.g. for AH/AL/AX/EAX,
     * this is RAX. This is needed when writing to a register
     * zeroes the complete parent register first (e.g. writing to EAX
     * zeroes upper half of RAX).
     */
    Register parent;
    const SubRegisterMask full;
    const SubRegisterMask *h;
    const SubRegisterMask *f;

    uint8_t sizeInBytes(void) const
    {
        switch (type) {
        case RegisterType::Flag1:
            /* we only model complete bytes */
            return 1;
        case RegisterType::Gprs8:
            return 1;
        case RegisterType::Gprs16:
            return 2;
        case RegisterType::Gprs32:
            return 4;
        case RegisterType::Gprs64:
            return 8;
        case RegisterType::Sse128:
            return 16;
        default:
            drob_assert_not_reached();
        }
    }
} RegisterInfo;

const SubRegisterMask& getSubRegisterMask(Register reg, RegisterAccessType access);
const SubRegisterMask& getSubRegisterMask(Register reg);

} /* namespace drob */

#endif /* REGISTER_INFO_HPP */
