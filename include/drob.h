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
#ifndef DROB_H
#define DROB_H

#ifdef __cplusplus
#include <cstdint>
#include <cstdbool>
#include <cstdio>
#else
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif
#include <xmmintrin.h>

typedef enum drob_param_type {
    DROB_PARAM_TYPE_VOID = 0,
    DROB_PARAM_TYPE_BOOL,
    DROB_PARAM_TYPE_CHAR, /* signed char */
    DROB_PARAM_TYPE_UCHAR,
    DROB_PARAM_TYPE_SHORT, /* signed int, enum */
    DROB_PARAM_TYPE_USHORT,
    DROB_PARAM_TYPE_INT,
    DROB_PARAM_TYPE_UINT,
    DROB_PARAM_TYPE_LONG, /* signed long */
    DROB_PARAM_TYPE_ULONG,
    DROB_PARAM_TYPE_LONGLONG, /* signed long long */
    DROB_PARAM_TYPE_ULONGLONG,
    DROB_PARAM_TYPE_INT8,
    DROB_PARAM_TYPE_INT16,
    DROB_PARAM_TYPE_INT32,
    DROB_PARAM_TYPE_INT64,
    DROB_PARAM_TYPE_UINT8,
    DROB_PARAM_TYPE_UINT16,
    DROB_PARAM_TYPE_UINT32,
    DROB_PARAM_TYPE_UINT64,
    DROB_PARAM_TYPE_INT128, /* signed __int128 */
    DROB_PARAM_TYPE_UINT128,
    DROB_PARAM_TYPE_FLOAT,
    DROB_PARAM_TYPE_DOUBLE,
    DROB_PARAM_TYPE_M128,
    DROB_PARAM_TYPE_FLOAT128,
    DROB_PARAM_TYPE_PTR,
    DROB_PARAM_TYPE_MAX,
} drob_param_type;

typedef enum drob_ptr_flag {
    /*
     * All values read directly via this pointer can be assumed to be
     * constant. Indirect read values (reading via a read pointer) are
     * not assumed to be constant. Put these into RO sections or mark
     * the ranges as constant.
     */
    DROB_PTR_FLAG_CONST = 0,
    /*
     * No other pointers will be used to access data accessed via this
     * pointer.
     */
    DROB_PTR_FLAG_RESTRICT,
    /* this pointer will never be NULL */
    DROB_PTR_FLAG_NOTNULL,
} drob_ptr_flag;

typedef enum drob_loglevel {
    DROB_LOGLEVEL_NONE = 0,
    DROB_LOGLEVEL_ERROR,
    DROB_LOGLEVEL_WARNING,
    DROB_LOGLEVEL_INFO,
    DROB_LOGLEVEL_DEBUG,
} drob_loglevel;

typedef enum drob_error_handling {
    DROB_ERROR_HANDLING_RETURN_NULL = 0,
    DROB_ERROR_HANDLING_RETURN_ORIGINAL,
    DROB_ERROR_HANDLING_ABORT,
} drob_error_handling;

struct drob_cfg;
typedef struct drob_cfg drob_cfg;
typedef void* drob_f;

/*
 * Setup drob. Has to be executed at most once.
 */
int drob_setup(void);

/*
 * Teardown drob.
 */
void drob_teardown(void);

/*
 * Configure logging. As default, all logging is disabled.
 * INFO and DEBUG might only be available in debug builds (to minimize
 * overhead).
 */
int drob_set_logging(FILE *file, drob_loglevel level);

/*
 * Create a new drob config, specifying the function definition.
 */
drob_cfg *drob_cfg_new(drob_param_type ret, unsigned int count, ...);
drob_cfg *drob_cfg_new0(drob_param_type ret, ...);
drob_cfg *drob_cfg_new1(drob_param_type ret, ...);
drob_cfg *drob_cfg_new2(drob_param_type ret, ...);
drob_cfg *drob_cfg_new3(drob_param_type ret, ...);
drob_cfg *drob_cfg_new4(drob_param_type ret, ...);
drob_cfg *drob_cfg_new5(drob_param_type ret, ...);

/*
 * Set a parameter to a known (constant) value.
 */
int drob_cfg_set_param_bool(drob_cfg *cfg, int nr, bool val);
int drob_cfg_set_param_char(drob_cfg *cfg, int nr, char val);
int drob_cfg_set_param_uchar(drob_cfg *cfg, int nr, unsigned char val);
int drob_cfg_set_param_short(drob_cfg *cfg, int nr, short val);
int drob_cfg_set_param_ushort(drob_cfg *cfg, int nr, unsigned short val);
int drob_cfg_set_param_int(drob_cfg *cfg, int nr, int val);
int drob_cfg_set_param_uint(drob_cfg *cfg, int nr, unsigned int val);
int drob_cfg_set_param_long(drob_cfg *cfg, int nr, long val);
int drob_cfg_set_param_ulong(drob_cfg *cfg, int nr, unsigned long val);
int drob_cfg_set_param_longlong(drob_cfg *cfg, int nr, long long val);
int drob_cfg_set_param_ulonglong(drob_cfg *cfg, int nr, unsigned long long val);
int drob_cfg_set_param_int8(drob_cfg *cfg, int nr, int8_t val);
int drob_cfg_set_param_int16(drob_cfg *cfg, int nr, int16_t val);
int drob_cfg_set_param_int32(drob_cfg *cfg, int nr, int32_t val);
int drob_cfg_set_param_int64(drob_cfg *cfg, int nr, int64_t val);
int drob_cfg_set_param_uint8(drob_cfg *cfg, int nr, uint8_t val);
int drob_cfg_set_param_uint16(drob_cfg *cfg, int nr, uint16_t val);
int drob_cfg_set_param_uint32(drob_cfg *cfg, int nr, uint32_t val);
int drob_cfg_set_param_uint64(drob_cfg *cfg, int nr, uint64_t val);
int drob_cfg_set_param_int128(drob_cfg *cfg, int nr, __int128 val);
int drob_cfg_set_param_uint128(drob_cfg *cfg, int nr, unsigned __int128 val);
int drob_cfg_set_param_float(drob_cfg *cfg, int nr, float val);
int drob_cfg_set_param_double(drob_cfg *cfg, int nr, double val);
int drob_cfg_set_param_m128(drob_cfg *cfg, int nr, __m128 val);
int drob_cfg_set_param_float128(drob_cfg *cfg, int nr, __float128 val);
int drob_cfg_set_param_ptr(drob_cfg *cfg, int nr, const void *val);

/*
 * Set flags for ptr values (helpful even if the actual value is not
 * known but e.g. we can assume it is never null).
 */
int drob_cfg_set_ptr_flag(drob_cfg *cfg, int nr, drob_ptr_flag flag);

/*
 * Set the minimum alignment of the pointer that is guaranteed, even though
 * the value of the pointer is not known. Has to be a power of two or 0.
 */
int drob_cfg_set_ptr_align(drob_cfg *cfg, int nr, uint16_t align);

/*
 * Set a memory range to be treated as if constant. (not necessary if
 * the memory is read-only already).
 */
int drob_cfg_add_const_range(drob_cfg *cfg, void *start, unsigned long size);

/*
 * Should all unmodelled instructions lead to a rewriting failure?
 */
void drob_cfg_fail_on_unmodelled(drob_cfg *cfg, bool fail);

/*
 * How often to unroll simple loops. Default is 10.
 */
void drob_cfg_set_simple_loop_unroll_count(drob_cfg *cfg, uint16_t count);

/*
 * How to proceed if rewriting failed?
 */
void drob_cfg_set_error_handling(drob_cfg *cfg, drob_error_handling handling);

/*
 * Free a drob configuration.
 */
void drob_cfg_free(drob_cfg *cfg);

/*
 * Dump a drob config to stdout.
 */
void drob_cfg_dump(const drob_cfg *cfg);

/*
 * Optimize a function using the given drob config.
 */
drob_f drob_optimize(drob_f func, const drob_cfg *cfg);

/*
 * Release an optimized function.
 */
void drob_free(drob_f func);

#ifdef __cplusplus
}
#endif

#endif /* DROB_H */
