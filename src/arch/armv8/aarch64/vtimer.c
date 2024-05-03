#include <vtimer.h>
#include <vm.h>
#include <timer.h>

void vtimer_init(struct vcpu* vcpu)
{

}

void vtimer_reset(struct vcpu* vcpu)
{
    vcpu->arch.vtimer.cntv_ctl_el0 = 0;
    vcpu->arch.vtimer.cntv_cval_el0 = 0;
    vcpu->arch.vtimer.cntvoff_el2 = 0;
}

void vtimer_save_state(struct vcpu* vcpu)
{
    vcpu->arch.vtimer.cntkctl_el1 = sysreg_cntkctl_el1_read();
    vcpu->arch.vtimer.cntv_ctl_el0 = sysreg_cntv_ctl_el0_read();
    vcpu->arch.vtimer.cntv_cval_el0 = sysreg_cntv_cval_el0_read();
    vgic_hw_irq_save_state(vcpu, VTIMER_IRQ_ID);
    gic_set_enable(VTIMER_IRQ_ID, false);
}

void vtimer_restore_state(struct vcpu* vcpu)
{
    sysreg_cntkctl_el1_write(vcpu->arch.vtimer.cntkctl_el1);
    sysreg_cntv_ctl_el0_write(vcpu->arch.vtimer.cntv_ctl_el0);
    sysreg_cntv_cval_el0_write(vcpu->arch.vtimer.cntv_cval_el0);
    sysreg_cntvoff_el2_write(vcpu->arch.vtimer.cntvoff_el2);
    vgic_hw_commit(vcpu, VTIMER_IRQ_ID);
}

struct timer_event* vtimer_get_event(struct vcpu* vcpu) {

    unsigned long cntv_ctl;
    unsigned long cntv_cval;
    if (cpu()->vcpu == vcpu) {
        cntv_ctl = sysreg_cntv_ctl_el0_read();
        cntv_cval = sysreg_cntv_cval_el0_read();
    } else {
        cntv_ctl = vcpu->arch.vtimer.cntv_ctl_el0;
        cntv_cval = vcpu->arch.vtimer.cntv_cval_el0;
    }

    if (vgic_int_get_enable_by_id(vcpu, VTIMER_IRQ_ID) && (cntv_ctl & 0x3) == 0x1) {
        vcpu->stack_timer_event.timer = cntv_cval;
        return &vcpu->stack_timer_event;
    }

    return NULL;
}
