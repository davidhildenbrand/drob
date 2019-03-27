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
#ifndef UTILS_HPP
#define UTILS_HPP

#include <stdexcept>
#include <iostream>
#include <cstdint>
#include <cstdarg>
#include <cinttypes>
#include <string>
#include <limits>
#include "drob_internal.h"

namespace drob {

#define likely(x) __builtin_expect((x), 1)
#define unlikely(x) __builtin_expect((x), 0)
#define IS_ALIGNED(val, size) (!((uint64_t)(val) & ((size) - 1)))
#define ALIGN_DOWN(val, size) ((uint64_t)(val) & ~((size) - 1))
#define ALIGN_UP(val, size) (ALIGN_DOWN((val) + (size) - 1, (size)))
#define IS_POWER_OF_2(val) ((val) && (!((val) & ((val) - 1))))
#define ARRAY_SIZE(array) (sizeof(array) == 0 ? 0 : sizeof(array) / sizeof(array[0]))
#define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))

enum class TriState {
    False = 0,
    True,
    Unknown,
};

static inline TriState& operator&=(TriState &lhs, const TriState &rhs)
{
    if (rhs == TriState::True && lhs == TriState::True)
        lhs = TriState::True;
    else if (rhs == TriState::False || lhs == TriState::False)
        lhs = TriState::False;
    return lhs;
}

static inline TriState& operator|=(TriState &lhs, const TriState &rhs)
{
    if (rhs == TriState::True || lhs == TriState::True)
        lhs = TriState::True;
    else if (rhs == TriState::Unknown || lhs == TriState::Unknown)
        lhs = TriState::Unknown;
    return lhs;
}

static inline void __attribute__((noreturn)) drob_throw(const std::string& msg)
{
    throw std::runtime_error(msg);
}

#define __drob_assert(msg)                        \
    drob_throw(__FILE__ ":" + std::to_string(__LINE__) + " - " + msg);

#define drob_assert(cond)                        \
    if (!(cond)) {                             \
        __drob_assert("drob_assert(" #cond ")");        \
    }

#define drob_assert_not_reached()                    \
    __drob_assert("drob_assert_not_reached()")

static inline void __drob_log_start(const char *level)
{
    fprintf(logfile, "drob: %s:\t", level);
}

static inline void __drob_log_print(const char *format, ...)
{
    va_list arglist;

    va_start(arglist, format);
    vfprintf(logfile, format, arglist);
    va_end(arglist);
}

static inline void __drob_log_end(void)
{
    fprintf(logfile, "\n");
}

#define drob_error(...) \
    do { \
        if (unlikely(loglevel >= DROB_LOGLEVEL_ERROR)) { \
            __drob_log_start("error"); \
            __drob_log_print(__VA_ARGS__); \
            __drob_log_end(); \
        } \
    } while (0)


#define drob_warn(...) \
    do { \
        if (unlikely(loglevel >= DROB_LOGLEVEL_WARNING)) { \
            __drob_log_start("warning"); \
            __drob_log_print(__VA_ARGS__); \
            __drob_log_end(); \
        } \
    } while (0)

#define drob_dump(...) \
    do { \
        __drob_log_start("dump"); \
        __drob_log_print(__VA_ARGS__); \
        __drob_log_end(); \
    } while (0)

#define drob_dump_start(...) \
    do { \
        __drob_log_start("dump"); \
        __drob_log_print(__VA_ARGS__); \
    } while (0)

#define drob_dump_continue(...) \
    do { \
        __drob_log_print(__VA_ARGS__); \
    } while (0)

#define drob_dump_end() \
    do { \
        __drob_log_end(); \
    } while (0)


#ifdef DEBUG
#define drob_info(...) \
    do { \
        if (unlikely(loglevel >= DROB_LOGLEVEL_INFO)) { \
            __drob_log_start("info"); \
            __drob_log_print(__VA_ARGS__); \
            __drob_log_end(); \
        } \
    } while (0)

#define drob_debug(...) \
    do { \
        if (unlikely(loglevel >= DROB_LOGLEVEL_DEBUG)) { \
            __drob_log_start("debug"); \
            __drob_log_print(__VA_ARGS__); \
            __drob_log_end(); \
        } \
    } while (0)
#else
#define drob_info(...)
#define drob_debug(...)
#endif

static inline bool isDisp32(int64_t val)
{
    return (int32_t)val == val;
}

} /* namespace drob */

#endif /* UTILS_HPP */
