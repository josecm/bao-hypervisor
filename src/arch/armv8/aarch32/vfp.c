#include <arch/vfp.h>
#include <string.h>
#include <arch/sysregs.h>

void vfp_reset(struct vfp* vfp) {
    memset(vfp->d, 0, sizeof(vfp->d));
    vfp->fpscr = 0;
    vfp->fpexc = 0;
}

void vfp_save_state(struct vfp* vfp) {

    __asm__ volatile(
        "vstmia %0, {d0-d15} \n\t"
        "vstmia %1, {d16-d31} \n\t"
        : : "r"(&vfp->d[0]), "r"(&vfp->d[16]) : "memory"
    );

    __asm__ volatile(
        "vmrs %0, fpscr\n\t"
        "vmrs %1, fpexc\n\t"
        : "=r"(vfp->fpscr), "=r"(vfp->fpexc)
    );
}


void vfp_restore_state(struct vfp* vfp) {

    __asm__ volatile(
        "vldmia %0, {d0-d15} \n\t"
        "vldmia %1, {d16-d31} \n\t"
        : : "r"(&vfp->d[0]), "r"(&vfp->d[16]) : "memory"
    );

    __asm__ volatile(
        "vmsr fpscr, %0\n\t"
        "vmsr fpexc, %1\n\t"
        : : "r"(vfp->fpscr), "r"(vfp->fpexc)
    );
}
