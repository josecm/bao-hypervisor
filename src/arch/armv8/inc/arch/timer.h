#ifndef TIMER_ARCH_H
#define TIMER_ARCH_H

#include <bao.h>
#include <platform.h>
#include <arch/sysregs.h>

typedef uint64_t timer_value_t;

#ifdef CONFIG_TIMER_ARCH_FREQ
#define TIMER_ARCH_FREQ()   ((timer_value_t)CONFIG_TIMER_ARCH_FREQ)
#else
#define TIMER_ARCH_FREQ() ((timer_value_t)sysreg_cntfrq_el0_read())
#endif

static inline irqid_t timer_arch_irq_id(void)  
{
    return (irqid_t)platform.arch.generic_timer.hyp_irq;
}

static inline void timer_arch_set(timer_value_t value)
{
    sysreg_cnthp_cval_el2_write(value);
}

static inline timer_value_t timer_arch_get_count(void)
{
    return sysreg_cntpct_el0_read();
}

static inline void timer_arch_disable(void)
{
    sysreg_cnthp_cval_el2_write(~((uint64_t)0));
}


static inline void timer_arch_init(void)
{
    unsigned long cnthp_ctl_el2 = sysreg_cnthp_ctl_el2_read();
    sysreg_cnthp_ctl_el2_write(cnthp_ctl_el2 | CNT_CTL_ENABLE);
    sysreg_cnthp_cval_el2_write(~((uint64_t)0));
}

#endif /* TIMER_ARCH_H */
