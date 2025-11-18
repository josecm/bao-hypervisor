#include <arch/vfp.h>
#include <string.h>
#include <arch/sysregs.h>

void vfp_reset(struct vfp* vfp) {
    memset(vfp->d, 0, sizeof(vfp->d));
    vfp->fpscr = 0;
    vfp->fpexc = 0;
}

void vfp_save_state(struct vfp* vfp) {

    vfp->fpexc = sysreg_fpexc_read();
    sysreg_fpexc_write(vfp->fpexc | (1UL << 30));
    vfp->fpscr = sysreg_fpscr_read();

    __asm__ volatile(
        "vstmia %0, {d0-d15} \n\t"
        "vstmia %1, {d16-d31} \n\t"
        : : "r"(&vfp->d[0]), "r"(&vfp->d[16]) : "memory"
    );

    sysreg_fpexc_write(vfp->fpexc);
}


void vfp_restore_state(struct vfp* vfp) {

    sysreg_fpexc_write(sysreg_fpexc_read()| (1UL << 30));

    __asm__ volatile(
        "vldmia %0, {d0-d15} \n\t"
        "vldmia %1, {d16-d31} \n\t"
        : : "r"(&vfp->d[0]), "r"(&vfp->d[16]) : "memory"
    );

    sysreg_fpscr_write(vfp->fpscr);
    sysreg_fpexc_write(vfp->fpexc);
}
