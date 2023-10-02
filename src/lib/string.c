/**
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (c) Bao Project and Contributors. All rights reserved.
 */

#include <string.h>

void *memcpy(void *dst, const void *src, size_t count)
{
    size_t i;
    uint8_t *dst_tmp = dst;
    const uint8_t *src_tmp = src;
    static const size_t WORD_SIZE = sizeof(unsigned long);

    if (!((uintptr_t)src & (WORD_SIZE - 1)) &&
        !((uintptr_t)dst & (WORD_SIZE - 1))) {
        for (i = 0; i < count; i += WORD_SIZE) {
            if (i + (WORD_SIZE - 1) > count - 1) break;
            *(unsigned long *)dst_tmp = *(unsigned long *)src_tmp;
            dst_tmp += WORD_SIZE;
            src_tmp += WORD_SIZE;
        }
        if (i <= count - 1) {
            for (; i < count; i++) {
                *dst_tmp = *src_tmp;
                dst_tmp++;
                src_tmp++;
            }
        }
    } else {
        for (i = 0; i < count; i++) dst_tmp[i] = src_tmp[i];
    }
    return dst;
}

void *memset(void *dest, int c, size_t count)
{
    uint8_t *d;
    d = (uint8_t *)dest;

    while (count--) {
        *d = c;
        d++;
    }

    return dest;
}

size_t strnlen(const char *s, size_t n)
{
    const char *str;

    for (str = s; *str != '\0' && n--; ++str) {
        /* Just iterate */
    }
    return str - s;
}
