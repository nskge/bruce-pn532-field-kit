#ifndef BRUCE_MFOC_ERR_COMPAT_H
#define BRUCE_MFOC_ERR_COMPAT_H

#include <stdarg.h>
#include <stdio.h>

static inline void warnx(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fputc('\n', stderr);
}

#endif
