/**
 * SPDX-License-Identifier: GPL-2.0
 * Copyright (c) Bao Project and Contributors. All rights reserved.
 */

#include <vmm.h>
#include <platform.h>
#include <arch/generic_timer.h>

void vmm_arch_profile_init()
{
    /** 
     * Since there is no firmware in cortex-r platforms, we need to
     * initialize the system counter.
     */
    volatile struct generic_timer_cntctrl *timer_ctl =
    (struct generic_timer_cntctrl* )mem_alloc_map_dev(&cpu()->as, SEC_HYP_PRIVATE,
                      platform.arch.generic_timer.base_addr,
                      platform.arch.generic_timer.base_addr,
                      sizeof(struct generic_timer_cntctrl));

    if (cpu()->id == CPU_MASTER) {
        timer_ctl->CNTCR |= GENERIC_TIMER_CNTCTL_CNTCR_EN;
    }

    sysreg_cntfrq_el0_write(timer_ctl->CNTDIF0);
}
