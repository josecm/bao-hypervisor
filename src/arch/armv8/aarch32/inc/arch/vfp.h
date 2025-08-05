#ifndef ARCH_VFP_H
#define ARCH_VFP_H

#include <types.h>

#define VFP_NUM_REGS    (32)

struct vfp {
    uint64_t d[VFP_NUM_REGS];
    uint32_t fpscr;
    uint32_t fpexc;
}  __attribute__((aligned(8)));

void vfp_reset(struct vfp* vfp);
void vfp_save_state(struct vfp* vfp);
void vfp_restore_state(struct vfp* vfp);

#endif /* ARCH_VFP_H */
