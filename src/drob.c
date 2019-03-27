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
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "drob_internal.h"

#define IS_POWER_OF_2(val) ((val) && (!((val) & ((val) - 1))))

int drob_setup(void)
{
    return drobcpp_setup();
}

void drob_teardown(void)
{
    drobcpp_teardown();
}

int drob_set_logging(FILE *file, drob_loglevel level)
{
    if (!file) {
        return -EINVAL;
    }
    switch (level) {
    case DROB_LOGLEVEL_NONE:
    case DROB_LOGLEVEL_ERROR:
    case DROB_LOGLEVEL_WARNING:
    case DROB_LOGLEVEL_INFO:
    case DROB_LOGLEVEL_DEBUG:
        break;
    default:
        return -EINVAL;
    }
    loglevel = level;
#ifndef DEBUG
    if (loglevel >= DROB_LOGLEVEL_WARNING)
        loglevel = DROB_LOGLEVEL_WARNING;
#endif
    logfile = file;
    return 0;
}

static drob_param_cfg *drob_param_cfg_new_va(unsigned int count, va_list args)
{
    drob_param_cfg *cfg, *cur;
    unsigned int i;

    cfg = malloc(count * sizeof(*cfg));
    if (!cfg) {
        return NULL;
    }

    for (i = 0; i < count; i++) {
        cur = &cfg[i];
        cur->type = va_arg(args, drob_param_type);
        if (cur->type >= DROB_PARAM_TYPE_MAX) {
            free(cfg);
            return NULL;
        }
        if (cur->type == DROB_PARAM_TYPE_VOID) {
            free(cfg);
            return NULL;
        }
        cur->state = DROB_PARAM_STATE_UNKNOWN;
    }

    return cfg;
}

static drob_cfg *drob_cfg_new_va(drob_param_type ret, unsigned int count,
                 va_list args)
{
    drob_cfg *cfg;

    cfg = malloc(sizeof(*cfg));
    if (!cfg) {
        return NULL;
    }
    memset(cfg, 0, sizeof(*cfg));

    /* set the return type */
    cfg->ret_type = ret;

    /* create the parameter list */
    cfg->param_count = count;
    cfg->params = drob_param_cfg_new_va(count, args);
    if (!cfg->params) {
        drob_cfg_free(cfg);
        return NULL;
    }

    /* set the simple loop unroll count */
    cfg->simple_loop_unroll_count = 10;

    return cfg;
}

drob_cfg *drob_cfg_new(drob_param_type ret, unsigned int count, ...)
{
    drob_cfg *cfg;
    va_list args;

    va_start(args, count);
    cfg = drob_cfg_new_va(ret, count, args);
    va_end(args);

    return cfg;
}

drob_cfg *drob_cfg_new0(drob_param_type ret, ...)
{
    drob_cfg *cfg;
    va_list args;

    va_start(args, ret);
    cfg = drob_cfg_new_va(ret, 0, args);
    va_end(args);

    return cfg;
}

drob_cfg *drob_cfg_new1(drob_param_type ret, ...)
{
    drob_cfg *cfg;
    va_list args;

    va_start(args, ret);
    cfg = drob_cfg_new_va(ret, 1, args);
    va_end(args);

    return cfg;
}

drob_cfg *drob_cfg_new2(drob_param_type ret, ...)
{
    drob_cfg *cfg;
    va_list args;

    va_start(args, ret);
    cfg = drob_cfg_new_va(ret, 2, args);
    va_end(args);

    return cfg;
}

drob_cfg *drob_cfg_new3(drob_param_type ret, ...)
{
    drob_cfg *cfg;
    va_list args;

    va_start(args, ret);
    cfg = drob_cfg_new_va(ret, 3, args);
    va_end(args);

    return cfg;
}

drob_cfg *drob_cfg_new4(drob_param_type ret, ...)
{
    drob_cfg *cfg;
    va_list args;

    va_start(args, ret);
    cfg = drob_cfg_new_va(ret, 4, args);
    va_end(args);

    return cfg;
}

drob_cfg *drob_cfg_new5(drob_param_type ret, ...)
{
    drob_cfg *cfg;
    va_list args;

    va_start(args, ret);
    cfg = drob_cfg_new_va(ret, 5, args);
    va_end(args);

    return cfg;
}

static const char *param_type_names[] = {
    [DROB_PARAM_TYPE_VOID] = "void", /* only valid as return type */
    [DROB_PARAM_TYPE_BOOL] = "bool",
    [DROB_PARAM_TYPE_CHAR] = "char",
    [DROB_PARAM_TYPE_UCHAR] = "unsigned char",
    [DROB_PARAM_TYPE_SHORT] = "short",
    [DROB_PARAM_TYPE_USHORT] = "unsigned short",
    [DROB_PARAM_TYPE_INT] = "int",
    [DROB_PARAM_TYPE_UINT] = "unsigned int",
    [DROB_PARAM_TYPE_LONG] = "long",
    [DROB_PARAM_TYPE_ULONG] = "unsigned long",
    [DROB_PARAM_TYPE_LONGLONG] = "long long",
    [DROB_PARAM_TYPE_ULONGLONG]= "unsigned long long",
    [DROB_PARAM_TYPE_INT8] = "int8_t",
    [DROB_PARAM_TYPE_INT16] = "int16_t",
    [DROB_PARAM_TYPE_INT32] = "int32_t",
    [DROB_PARAM_TYPE_INT64] = "int64_t",
    [DROB_PARAM_TYPE_UINT8] = "uint8_t",
    [DROB_PARAM_TYPE_UINT16] = "uint16_t",
    [DROB_PARAM_TYPE_UINT32] = "uint32_t",
    [DROB_PARAM_TYPE_UINT64] = "uint64_t",
    [DROB_PARAM_TYPE_INT128] = "__int128",
    [DROB_PARAM_TYPE_UINT128] = "unsigned __int128",
    [DROB_PARAM_TYPE_FLOAT] = "float",
    [DROB_PARAM_TYPE_DOUBLE] = "double",
    [DROB_PARAM_TYPE_M128] = "__m128",
    [DROB_PARAM_TYPE_FLOAT128] = "__float128",
    [DROB_PARAM_TYPE_PTR] = "void *",

};

void drob_cfg_dump(const drob_cfg *cfg)
{
    const drob_param_cfg *cur;
    bool is_first = true;
    int i;

    /* dump the function declaration */
    printf("%s (*func)(", param_type_names[cfg->ret_type]);

    for (i = 0; i < cfg->param_count; i++) {
        cur = &cfg->params[i];
        if (is_first) {
            printf("%s", param_type_names[cur->type]);
            is_first = false;
        } else {
            printf(", %s", param_type_names[cur->type]);
        }
    }

    printf(");\n");
}

#define DEF_DROB_CFG_SET_PARAM(_type, _tn_uc, _tn_lc) \
    int drob_cfg_set_param_##_tn_lc(drob_cfg *cfg, int nr, _type val) \
    { \
        if (nr > cfg->param_count) { \
            return -EINVAL; \
        } \
        if (cfg->params[nr].type != DROB_PARAM_TYPE_##_tn_uc) { \
            return -EINVAL; \
        }\
        cfg->params[nr].value._tn_lc##_val = val; \
        cfg->params[nr].state = DROB_PARAM_STATE_CONST; \
        return 0; \
    }

DEF_DROB_CFG_SET_PARAM(_Bool, BOOL, bool)
DEF_DROB_CFG_SET_PARAM(char, CHAR, char)
DEF_DROB_CFG_SET_PARAM(unsigned char, UCHAR, uchar)
DEF_DROB_CFG_SET_PARAM(short, SHORT, short)
DEF_DROB_CFG_SET_PARAM(unsigned short, USHORT, ushort)
DEF_DROB_CFG_SET_PARAM(int, INT, int)
DEF_DROB_CFG_SET_PARAM(unsigned int, UINT, uint)
DEF_DROB_CFG_SET_PARAM(long, LONG, long)
DEF_DROB_CFG_SET_PARAM(unsigned long, ULONG, ulong)
DEF_DROB_CFG_SET_PARAM(long long, LONGLONG, longlong)
DEF_DROB_CFG_SET_PARAM(unsigned long long, ULONGLONG, ulonglong)
DEF_DROB_CFG_SET_PARAM(int8_t, INT8, int8)
DEF_DROB_CFG_SET_PARAM(int16_t, INT16, int16)
DEF_DROB_CFG_SET_PARAM(int32_t, INT32, int32)
DEF_DROB_CFG_SET_PARAM(int64_t, INT64, int64)
DEF_DROB_CFG_SET_PARAM(uint8_t, UINT8, uint8)
DEF_DROB_CFG_SET_PARAM(uint16_t, UINT16, uint16)
DEF_DROB_CFG_SET_PARAM(uint32_t, UINT32, uint32)
DEF_DROB_CFG_SET_PARAM(uint64_t, UINT64, uint64)
DEF_DROB_CFG_SET_PARAM(__int128, INT128, int128)
DEF_DROB_CFG_SET_PARAM(unsigned __int128, UINT128, uint128)
DEF_DROB_CFG_SET_PARAM(float, FLOAT, float);
DEF_DROB_CFG_SET_PARAM(double, DOUBLE, double);
DEF_DROB_CFG_SET_PARAM(__m128, M128, m128);
DEF_DROB_CFG_SET_PARAM(__float128, FLOAT128, float128);
DEF_DROB_CFG_SET_PARAM(const void *, PTR, ptr);

int drob_cfg_set_ptr_flag(drob_cfg *cfg, int nr, drob_ptr_flag flag)
{
    if (nr > cfg->param_count) {
        return -EINVAL;
    }
    if (cfg->params[nr].type != DROB_PARAM_TYPE_PTR) {
        return -EINVAL;
    }
    switch (flag) {
    case DROB_PTR_FLAG_CONST:
        cfg->params[nr].ptr_flags |= 1ULL << DROB_PTR_FLAG_CONST;
        break;
    case DROB_PTR_FLAG_RESTRICT:
        cfg->params[nr].ptr_flags |= 1ULL << DROB_PTR_FLAG_RESTRICT;
        break;
    case DROB_PTR_FLAG_NOTNULL:
        cfg->params[nr].ptr_flags |= 1ULL << DROB_PTR_FLAG_NOTNULL;
        break;
    default:
        return -EINVAL;
    }
    return 0;
}

int drob_cfg_set_ptr_align(drob_cfg *cfg, int nr, uint16_t align)
{
    if (nr > cfg->param_count) {
        return -EINVAL;
    }
    if (cfg->params[nr].type != DROB_PARAM_TYPE_PTR) {
        return -EINVAL;
    }
    if (align != 0 && !IS_POWER_OF_2(align)) {
        return -EINVAL;
    }
    cfg->params[nr].ptr_align = align;
    return 0;
}

int drob_cfg_add_const_range(drob_cfg *cfg, void *start, unsigned long size)
{
    drob_mem_cfg *tmp;

    if (cfg->ranges) {
        tmp = realloc(cfg->ranges,
                  (cfg->range_count + 1) * sizeof(*cfg->ranges));
        if (!tmp) {
            return -ENOMEM;
        }
        cfg->ranges = tmp;
        cfg->range_count++;
    } else {
        cfg->ranges = malloc(sizeof(*cfg->ranges));
        if (!cfg->ranges) {
            return -ENOMEM;
        }
        cfg->range_count = 1;
    }
    cfg->ranges[cfg->range_count - 1].start = start;
    cfg->ranges[cfg->range_count - 1].size = size;
    return 0;
}

void drob_cfg_fail_on_unmodelled(drob_cfg *cfg, bool fail)
{
    cfg->fail_on_unmodelled = fail;
}

void drob_cfg_set_simple_loop_unroll_count(drob_cfg *cfg, uint16_t count)
{
    cfg->simple_loop_unroll_count = count;
}


void drob_cfg_set_error_handling(drob_cfg *cfg, drob_error_handling handling)
{
    cfg->error_handling = handling;
}

void drob_cfg_free(drob_cfg *cfg)
{
    if (!cfg) {
        return;
    }
    if (cfg->params) {
        free(cfg->params);
    }
    if (cfg->ranges) {
        free(cfg->ranges);
    }
    free(cfg);
}

void drob_free(drob_f func)
{
    drobcpp_free((const uint8_t *)func);
}

drob_f drob_optimize(drob_f func, const drob_cfg *cfg)
{
    const uint8_t *ftext = (const uint8_t *)func;

    return (drob_f)drobcpp_optimize(ftext, cfg);
}
