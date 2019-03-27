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
#ifndef _UTIL_BITMAP_H
#define _UTIL_BITMAP_H

#include <string.h>
#include <stdbool.h>

typedef unsigned long bitmap_t;

#define BITMAP_BITS_PER_ELEMENT (sizeof(bitmap_t) * 8)
#define BITMAP_BITS_TO_ELEMENTS(bits) \
    (((bits) + BITMAP_BITS_PER_ELEMENT - 1) / BITMAP_BITS_PER_ELEMENT)
#define BITMAP_BITS_TO_BYTES(bits) \
    (BITMAP_BITS_TO_ELEMENTS(bits) * sizeof(bitmap_t))

#define BITMAP_BIT_ELEMENT(bmap, bit) bmap[(bit) / BITMAP_BITS_PER_ELEMENT]
#define BITMAP_BIT_ELEMENT_MASK(bit) (1ul << ((bit) % BITMAP_BITS_PER_ELEMENT))

#define BITMAP_LAST_ELEMENT_BITS(bits) (bits % BITMAP_BITS_PER_ELEMENT)
#define BITMAP_LAST_ELEMENT_MASK(bits) \
    (-1ul >> (BITMAP_BITS_PER_ELEMENT - BITMAP_LAST_ELEMENT_BITS(bits)))

#define BITMAP_DECLARE(name, bits) \
        bitmap_t name[BITMAP_BITS_TO_ELEMENTS(bits)]

static inline void bitmap_zero(bitmap_t dst[], unsigned long bits)
{
    const unsigned long bytes = BITMAP_BITS_TO_BYTES(bits);

    memset(dst, 0, bytes);
}

static inline void bitmap_fill(bitmap_t dst[], unsigned long bits)
{
    const unsigned long elements = BITMAP_BITS_TO_ELEMENTS(bits);
    const unsigned long bytes = BITMAP_BITS_TO_BYTES(bits);

    memset(dst, 0xff,  bytes);
    dst[elements - 1] = BITMAP_LAST_ELEMENT_MASK(bits);
}

void bitmap_andnot(bitmap_t dst[], const bitmap_t src1[], const bitmap_t src2[],
                   unsigned long bits);
void bitmap_or(bitmap_t dst[], const bitmap_t src1[], const bitmap_t src2[],
               unsigned long bits);
void bitmap_and(bitmap_t dst[], const bitmap_t src1[], const bitmap_t src2[],
                unsigned long bits);
bool bitmap_equal(const bitmap_t src1[], const bitmap_t src2[],
                  unsigned long bits);
bool bitmap_empty(const bitmap_t bmap[], unsigned long bits);

static inline void bitmap_set_bit(bitmap_t bmap[], unsigned long bit)
{
    BITMAP_BIT_ELEMENT(bmap, bit) |= BITMAP_BIT_ELEMENT_MASK(bit);
}

#endif /* _UTIL_BITMAP_H */
