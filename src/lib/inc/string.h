/**
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (c) Bao Project and Contributors. All rights reserved.
 */

#ifndef __STRING_H_
#define __STRING_H_

#include <bao.h>

void *memcpy(void *dst, const void *src, size_t count);
void *memset(void *dest, int c, size_t count);

size_t strnlen(const char *s, size_t n);

#endif /* __STRING_H_ */
