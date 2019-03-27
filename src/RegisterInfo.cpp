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
#include <iomanip>

#include "arch.hpp"
#include "RegisterInfo.hpp"

namespace drob {

void SubRegisterMask::dump(void) const
{
    unsigned int i;

    drob_dump_start("    ");
    for (i = 0; i < ARRAY_SIZE(m); ++i) {
        if (i) {
            drob_dump_continue(", ");
        }
        drob_dump_continue("%016llx", m[i]);
    }
    drob_dump_end();
}

const SubRegisterMask& getSubRegisterMask(Register reg, RegisterAccessType access)
{
    const RegisterInfo *info = arch_get_register_info(reg);

    switch (access) {
    case RegisterAccessType::FullZeroParent:
        return getSubRegisterMask(info->parent);
    case RegisterAccessType::Full:
        return info->full;
    case RegisterAccessType::H0:
        return info->h[0];
    case RegisterAccessType::H1:
        return info->h[1];
    case RegisterAccessType::F0:
        return info->f[0];
    case RegisterAccessType::F1:
        return info->f[1];
    case RegisterAccessType::F2:
        return info->f[2];
    case RegisterAccessType::F3:
        return info->f[3];
    default:
        drob_assert_not_reached();
    }
}

const SubRegisterMask& getSubRegisterMask(Register reg)
{
    return getSubRegisterMask(reg, RegisterAccessType::Full);
}

}
