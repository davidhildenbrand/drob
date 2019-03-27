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
#ifndef DROB_INTERNAL_H
#define DROB_INTERNAL_H

#include "drob.h"

#ifdef __cplusplus
#include <cstdint>
#else
#include <stdint.h>
#endif

#ifdef __cplusplus
extern "C" {
namespace drob {
#endif

extern drob_loglevel loglevel;
extern FILE *logfile;

/*
 * Parameter and memory definitions: Specify runtime information of
 * parameters.
 */
typedef union drob_param_value {
    _Bool bool_val;
    char char_val;
    unsigned char uchar_val;
    short short_val;
    unsigned short ushort_val;
    int int_val;
    unsigned int uint_val;
    long long_val;
    unsigned long ulong_val;
    long long longlong_val;
    unsigned long long ulonglong_val;
    int8_t int8_val;
    int16_t int16_val;
    int32_t int32_val;
    int64_t int64_val;
    uint8_t uint8_val;
    uint16_t uint16_val;
    uint32_t uint32_val;
    uint64_t uint64_val;
    __int128 int128_val;
    unsigned __int128 uint128_val;
    float float_val;
    double double_val;
    __m128 m128_val;
    __float128 float128_val;
    const void *ptr_val;
    /*
     * Do we need size_t?
     *
     * x86-64:
     * - Enums are passed as int
     * - Not supported for now
     * -- long double (no x87 support)
     * -- _Decimal* (not required)
     * -- __m64 (no MMX/3DNOW support)
     * -- __m256 (no AVX support yet)
     * structs are in general not supported (some might be by modeling
     * passing members separatly)
     */
} drob_param_value;

typedef enum drob_param_state {
    DROB_PARAM_STATE_UNKNOWN = 0,
    DROB_PARAM_STATE_CONST,
} drob_param_state;

typedef struct drob_param_cfg {
    drob_param_type type;
    drob_param_state state;
    drob_param_value value;
    unsigned long long ptr_flags;
    uint16_t ptr_align;
} drob_param_cfg;

typedef struct drob_mem_cfg {
    void* start;
    unsigned long size;
} drob_mem_cfg;

typedef struct drob_cfg {
    /* the return type of the function */
    drob_param_type ret_type;
    /* the number of parameters */
    int param_count;
    /* information about parameters (type, runtime information ...) */
    drob_param_cfg *params;
    /* the number of memory ranges */
    int range_count;
    /* information about memory ranges (e.g. constant) */
    drob_mem_cfg *ranges;
    /* rewriting options */
    bool fail_on_unmodelled;
    drob_error_handling error_handling;
    uint16_t simple_loop_unroll_count;
} drob_cfg;

int drobcpp_setup(void);
void drobcpp_teardown(void);
const uint8_t *drobcpp_optimize(const uint8_t *ftext, const drob_cfg *cfg);
void drobcpp_free(const uint8_t *ftext);

#ifdef __cplusplus
}
}
#endif

#endif /* DROB_INTERNAL_H */
