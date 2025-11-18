/**
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (c) Bao Project and Contributors. All rights reserved.
 */

#ifndef __ABORTS_H__
#define __ABORTS_H__

#include <bao.h>

void aborts_sync_handler(void);
void internal_abort_handler(unsigned long gprs[]);
bool aborts_vcpu_inject_data_abort(unsigned long iss, unsigned long far, unsigned long il, unsigned long ec);
bool aborts_vcpu_inject_inst_abort(unsigned long iss, unsigned long far, unsigned long il, unsigned long ec);
bool aborts_vcpu_inject_undef_abort(unsigned long iss, unsigned long far, unsigned long il, unsigned long ec);

#endif /* __ABORTS_H__ */
