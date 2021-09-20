/* SPDX-License-Identifier: Apache-2.0
 * Copyright (c) 2021, Silicon Labs
 * Main authors:
 *     - Jérôme Pouiller <jerome.pouiller@silabs.com>
 */
#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>

#define __PRINT(color, msg, ...) \
    fprintf(stderr, "[" #color "m" msg "[0m\n", ##__VA_ARGS__)

#define __PRINT_WITH_LINE(color, msg, ...) \
    __PRINT(color, "%s():%d: " msg, __func__, __LINE__, ##__VA_ARGS__)

#define __PRINT_WITH_TIME(color, msg, ...) \
    do {                                                             \
        struct timespec tp;                                          \
        clock_gettime(CLOCK_REALTIME, &tp);                          \
        __PRINT(color, "%jd.%06jd: " msg, tp.tv_sec, tp.tv_nsec / 1000, ##__VA_ARGS__); \
    } while (0)

extern unsigned int g_enabled_traces;

enum {
    TR_RF   = 0x01,
    TR_CHAN = 0x02,
    TR_BUS  = 0x04,
    TR_HDLC = 0x08,
    TR_HIF  = 0x10,
};

#define __TRACE(COND, MSG, ...) \
    do {                                                             \
        if (g_enabled_traces & (COND)) {                             \
            if (MSG[0] != '\0')                                      \
                __PRINT_WITH_TIME(90, MSG, ##__VA_ARGS__);           \
            else                                                     \
                __PRINT_WITH_TIME(90, "%s:%d", __FILE__, __LINE__);  \
        }                                                            \
    } while (0)
#define TRACE(COND, ...)  __TRACE(COND, "" __VA_ARGS__)

#define __DEBUG(MSG, ...) \
    do {                                                             \
        if (MSG[0] != '\0')                                          \
            __PRINT_WITH_LINE(94, MSG, ##__VA_ARGS__);               \
        else                                                         \
            __PRINT_WITH_LINE(94, "trace");                          \
    } while (0)
#define DEBUG(...)  __DEBUG("" __VA_ARGS__)

#define INFO(MSG, ...) \
    do {                                                             \
        __PRINT(0, MSG, ##__VA_ARGS__);                              \
    } while (0)

#define __WARN(MSG, ...) \
    do {                                                             \
        if (MSG[0] != '\0')                                          \
            __PRINT(93, "warning: " MSG, ##__VA_ARGS__);             \
        else                                                         \
            __PRINT_WITH_LINE(93, "warning");                        \
    } while (0)
#define WARN(...)  __WARN("" __VA_ARGS__)

#define __WARN_ON(COND, MSG, ...) \
    ({                                                               \
        typeof(COND) __ret = (COND);                                 \
        if (__ret) {                                                 \
            if (MSG[0] != '\0')                                      \
                __PRINT(93, "warning: " MSG, ##__VA_ARGS__);         \
            else                                                     \
                __PRINT_WITH_LINE(93, "warning: \"%s\"", #COND);     \
        }                                                            \
        __ret;                                                       \
    })
#define WARN_ON(COND, ...) __WARN_ON(COND, "" __VA_ARGS__)

#define __FATAL(CODE, MSG, ...) \
    do {                                                             \
        if (MSG[0] != '\0')                                          \
            __PRINT(31, MSG, ##__VA_ARGS__);                         \
        else                                                         \
            __PRINT_WITH_LINE(31, "fatal error");                    \
        exit(CODE);                                                  \
    } while (0)
#define FATAL(CODE, ...) __FATAL(CODE, "" __VA_ARGS__)

#define __FATAL_ON(COND, CODE, MSG, ...) \
    do {                                                             \
        if (COND) {                                                  \
            if (MSG[0] != '\0')                                      \
                __PRINT(31, MSG, ##__VA_ARGS__);                     \
            else                                                     \
                __PRINT_WITH_LINE(31, "fatal error: \"%s\"", #COND); \
            exit(CODE);                                              \
        }                                                            \
    } while (0)
#define FATAL_ON(COND, CODE, ...) __FATAL_ON(COND, CODE, "" __VA_ARGS__)

#define __BUG(MSG, ...) \
    do {                                                             \
        if (MSG[0] != '\0')                                          \
            __PRINT_WITH_LINE(91, "bug: " MSG, ##__VA_ARGS__);       \
        else                                                         \
            __PRINT_WITH_LINE(91, "bug");                            \
        raise(SIGTRAP);                                              \
    } while (0)
#define BUG(...) __BUG("" __VA_ARGS__)

#define __BUG_ON(COND, MSG, ...) \
    do {                                                             \
        if (COND) {                                                  \
            if (MSG[0] != '\0')                                      \
                __PRINT_WITH_LINE(91, "bug: " MSG, ##__VA_ARGS__);   \
            else                                                     \
                __PRINT_WITH_LINE(91, "bug: \"%s\"", #COND);         \
            raise(SIGTRAP);                                          \
        }                                                            \
    } while (0)
#define BUG_ON(COND, ...) __BUG_ON(COND, "" __VA_ARGS__)

enum bytes_str_options {
    DELIM_SPACE     = 0x01, // Add space between each bytes
    DELIM_COLON     = 0x02, // Add colon between each bytes
    ELLIPSIS_ABRT   = 0x04, // Assert if output is too small
    ELLIPSIS_STAR   = 0x08, // End output with * if too small
    ELLIPSIS_DOTS   = 0x10, // End output with ... if too small
};
char *bytes_str(const void *in_start, int in_len, const void **in_done, char *out_start, int out_len, int opt);

#endif
