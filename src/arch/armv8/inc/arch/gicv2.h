/**
 * Bao, a Lightweight Static Partitioning Hypervisor
 *
 * Copyright (c) Bao Project (www.bao-project.org), 2019-
 *
 * Authors:
 *      Jose Martins <jose.martins@bao-project.org>
 *
 * Bao is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License version 2 as published by the Free
 * Software Foundation, with a special exception exempting guest code from such
 * license. See the COPYING file in the top-level directory for details.
 *
 */

#ifndef __GICV2_H__
#define __GICV2_H__

#include <arch/gic.h>

/**
 * @brief Reads the contents of a list register
 * 
 * @param i the index of the list register to read
 * @return uint64_t contents of the list register
 */
static inline uint64_t gich_read_lr(size_t i)
{
    if (i < NUM_LRS) {
        return gich.LR[i];
    } else {
        ERROR("gic: trying to read inexistent list register");
    }
}

/**
 * @brief Write to a list register
 * 
 * @param i the index of the list register to write
 * @param val the value to write in the list register
 */
static inline void gich_write_lr(size_t i, uint64_t val)
{
    if (i < NUM_LRS) {
        gich.LR[i] = val;
    } else {
        ERROR("gic: trying to write inexistent list register");
    }
}

/**
 * @brief Read the GICH.HCR register
 * 
 * @return uint32_t contents of the GICH.HCR register
 */
static inline uint32_t gich_get_hcr()
{
    return gich.HCR;
}

/**
 * @brief Set the contents of the GICH.HCR register
 * 
 * @param hcr the value to write in the GICH.HCR register
 */
static inline void gich_set_hcr(uint32_t hcr)
{
    gich.HCR = hcr;
}

/**
 * @brief Read the GICH.MISR register
 * 
 * @return uint32_t contents of the GICH.MISR register
 */
static inline uint32_t gich_get_misr()
{
    return gich.MISR;
}

/**
 * @brief Read the GICH.EISR register
 * 
 * @return uint64_t contents of the GICH.EISR register
 */
static inline uint64_t gich_get_eisr()
{
    uint64_t eisr = gich.EISR[0];
    if (NUM_LRS > 32) eisr |= (((uint64_t)gich.EISR[1] << 32));
    return eisr;
}

/**
 * @brief Read the GICH.EISR register
 * 
 * @return uint64_t contents of the GICH.EISR register
 */
static inline uint64_t gich_get_elrsr()
{
    uint64_t elsr = gich.ELSR[0];
    if (NUM_LRS > 32) elsr |= (((uint64_t)gich.ELSR[1] << 32));
    return elsr;
}

/**
 * @brief Read the GICC.IAR register, acknowleging the highest priority 
 * pending interrupt
 * 
 * @return uint64_t contents of the GICC.IAR register (ie. id highest enabled
 * pending plus other metadata)
 * 
 * @note IAR is non-idempotent, reading its contents will make the highest
 * enabled pending interrupt, active
 */
static inline uint32_t gicc_iar() {
    return gicc.IAR;
}

/**
 * @brief Write the GICC.EOIR register
 * 
 * @param eoir contents to write to GICC.EOIR register (should be the interrupt
 * id previsouly acknowledge using gicc_iar)
 * 
 * @note EOR is write-only, writing the id interrupt to it will make it go
 * from an active to an inactive state or decrease its priority (depending 
 * on if GICC.CTLR.EOImode is set, which by default, it is)
 */
static inline void gicc_eoir(uint32_t eoir) {
     gicc.EOIR = eoir;
}

/**
 * @brief Write the GICC.DIR register
 * 
 * @param dir contents to write to GICC.DIR register
 * 
 * @note GICC.DIR is a write-only register that deactivates the
 */
static inline void gicc_dir(uint32_t dir) {
     gicc.DIR = dir;
}

#endif /* __GICV2_H__ */
