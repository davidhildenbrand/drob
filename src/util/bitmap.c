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
#include "util/bitmap.h"

void bitmap_andnot(bitmap_t dst[], const bitmap_t src1[], const bitmap_t src2[],
                   unsigned long bits)
{
    const unsigned long elements = BITMAP_BITS_TO_ELEMENTS(bits);
    unsigned long i;

    for (i = 0; i < elements; i++) {
        dst[i] = src1[i] & ~src2[i];
    }
}

void bitmap_or(bitmap_t dst[], const bitmap_t src1[], const bitmap_t src2[],
               unsigned long bits)
{
    const unsigned long elements = BITMAP_BITS_TO_ELEMENTS(bits);
    unsigned long i;

    for (i = 0; i < elements; i++) {
        dst[i] = src1[i] | src2[i];
    }
}

void bitmap_and(bitmap_t dst[], const bitmap_t src1[], const bitmap_t src2[],
                unsigned long bits)
{
    const unsigned long elements = BITMAP_BITS_TO_ELEMENTS(bits);
    unsigned long i;

    for (i = 0; i < elements; i++) {
        dst[i] = src1[i] & src2[i];
    }
}

bool bitmap_equal(const bitmap_t src1[], const bitmap_t src2[],
                  unsigned long bits)
{
    const unsigned long elements = BITMAP_BITS_TO_ELEMENTS(bits);
    unsigned long i;

    if (elements > 0) {
        for (i = 0; i < elements - 1; i++) {
            if (src1[i] != src2[i])
                return false;
        }
    }
    return ((src1[elements - 1] & BITMAP_LAST_ELEMENT_MASK(bits)) ==
            (src2[elements - 1] & BITMAP_LAST_ELEMENT_MASK(bits)));
}

bool bitmap_empty(const bitmap_t bmap[], unsigned long bits)
{
    const unsigned long elements = BITMAP_BITS_TO_ELEMENTS(bits);
    unsigned long i;

    if (elements > 0) {
        for (i = 0; i < elements - 1; i++) {
            if (bmap[i])
                return false;
        }
    }
    return !(bmap[elements - 1] & BITMAP_LAST_ELEMENT_MASK(bits));
}
