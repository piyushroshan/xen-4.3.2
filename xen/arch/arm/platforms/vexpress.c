/*
 * xen/arch/arm/platform_vexpress.c
 *
 * Versatile Express specific settings
 *
 * Stefano Stabellini <stefano.stabellini@eu.citrix.com>
 * Copyright (c) 2013 Citrix Systems.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <asm/platforms/vexpress.h>
#include <asm/platform.h>
#include <xen/mm.h>
#include <xen/vmap.h>

#define DCC_SHIFT      26
#define FUNCTION_SHIFT 20
#define SITE_SHIFT     16
#define POSITION_SHIFT 12
#define DEVICE_SHIFT   0

static inline int vexpress_ctrl_start(uint32_t *syscfg, int write,
                                      int function, int device)
{
    int dcc = 0; /* DCC to access */
    int site = 0; /* motherboard */
    int position = 0; /* daughterboard */
    uint32_t stat;

    /* set control register */
    syscfg[V2M_SYS_CFGCTRL/4] = V2M_SYS_CFG_START |
        (write ? V2M_SYS_CFG_WRITE : 0) |
        (dcc << DCC_SHIFT) | (function << FUNCTION_SHIFT) |
        (site << SITE_SHIFT) | (position << POSITION_SHIFT) |
        (device << DEVICE_SHIFT);

    /* wait for complete flag to be set */
    do {
        stat = syscfg[V2M_SYS_CFGSTAT/4];
        dsb();
    } while ( !(stat & V2M_SYS_CFG_COMPLETE) );

    /* check error status and return error flag if set */
    if ( stat & V2M_SYS_CFG_ERROR )
    {
        printk(KERN_ERR "V2M SYS_CFGSTAT reported a configuration error\n");
        return -1;
    }
    return 0;
}

int vexpress_syscfg(int write, int function, int device, uint32_t *data)
{
    uint32_t *syscfg = (uint32_t *) FIXMAP_ADDR(FIXMAP_MISC);
    int ret = -1;

    set_fixmap(FIXMAP_MISC, V2M_SYS_MMIO_BASE >> PAGE_SHIFT, DEV_SHARED);

    if ( syscfg[V2M_SYS_CFGCTRL/4] & V2M_SYS_CFG_START )
        goto out;

    /* clear the complete bit in the V2M_SYS_CFGSTAT status register */
    syscfg[V2M_SYS_CFGSTAT/4] = 0;

    if ( write )
    {
        /* write data */
        syscfg[V2M_SYS_CFGDATA/4] = *data;

        if ( vexpress_ctrl_start(syscfg, write, function, device) < 0 )
            goto out;
    } else {
        if ( vexpress_ctrl_start(syscfg, write, function, device) < 0 )
            goto out;
        else
            /* read data */
            *data = syscfg[V2M_SYS_CFGDATA/4];
    }

    ret = 0;
out:
    clear_fixmap(FIXMAP_MISC);
    return ret;
}

/*
 * TODO: Get base address from the device tree
 * See arm,vexpress-reset node
 */
static void vexpress_reset(void)
{
    void __iomem *sp810;

    /* Use the SP810 system controller to force a reset */
    sp810 = ioremap_nocache(SP810_ADDRESS, PAGE_SIZE);

    if ( !sp810 )
    {
        dprintk(XENLOG_ERR, "Unable to map SP810\n");
        return;
    }

    /* switch to slow mode */
    iowritel(sp810, 0x3);
    dsb(); isb();
    /* writing any value to SCSYSSTAT reg will reset the system */
    iowritel(sp810 + 4, 0x1);
    dsb(); isb();

    iounmap(sp810);
}

static const char const *vexpress_dt_compat[] __initdata =
{
    "arm,vexpress",
    NULL
};

PLATFORM_START(vexpress, "VERSATILE EXPRESS")
    .compatible = vexpress_dt_compat,
    .reset = vexpress_reset,
PLATFORM_END

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
