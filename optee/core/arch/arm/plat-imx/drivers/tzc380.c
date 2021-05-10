// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright 2019 Pengutronix
 * All rights reserved.
 *
 * Rouven Czerwinski <entwicklung@pengutronix.de>
 */

#include <config.h>
#include <drivers/tzc380.h>
#include <imx-regs.h>
#include <initcall.h>
#include <kernel/panic.h>
#include <mm/core_memprot.h>
#include <mm/generic_ram_layout.h>

/*
 * TZASC2_BASE is asserted non null when used.
 * This is needed to compile the code for i.MX6UL/L
 * and i.MX8MQ.
 */
#ifndef TZASC2_BASE
#define TZASC2_BASE			0
#endif

#define IPU_BASE            0x02400000 
#define CAAM_BASE           0x00100000
#define IPU_CM_OFFSET       0x00200000
#define IPU_CPMEM_OFFSET    0x00300000
#define IPU_CHAN_27         27

#define IPUx_DP_Graph_Wind_CTRL_SYNC    0x18004
#define IPUx_DP_COM_CONF_SYNC           0x18000
#define IPUx_IDMAC_CH_EN_1              0x08004



static uint32_t ipu_ch_param_addr(paddr_t base, int channel, int w, int bit, int size) {
    int i = bit / 32;
    int reg_offset = channel * 64;
    reg_offset += w * 32;
    reg_offset += i;

    return base + reg_offset;
}


static TEE_Result imx_configure_tzasc(void)
{
	vaddr_t addr[2] = {0};
	int end = 1;
	int i = 0;

	addr[0] = core_mmu_get_va(TZASC_BASE, MEM_AREA_IO_SEC);

	if (IS_ENABLED(CFG_MX6Q) || IS_ENABLED(CFG_MX6D) ||
	    IS_ENABLED(CFG_MX6DL)) {
		assert(TZASC2_BASE != 0);
		addr[1] = core_mmu_get_va(TZASC2_BASE, MEM_AREA_IO_SEC);
		end = 2;
	}

	for (i = 1; i < end; i++) {
        uint8_t region = 1;
		tzc_init(addr[i]);

		region = tzc_auto_configure(CFG_DRAM_BASE, CFG_DDR_SIZE,
			     TZC_ATTR_SP_NS_RW, region);
		region = tzc_auto_configure(CFG_TZDRAM_START, CFG_TZDRAM_SIZE,
			     TZC_ATTR_SP_S_RW, region);
		region = tzc_auto_configure(CFG_SHMEM_START, CFG_SHMEM_SIZE,
			     TZC_ATTR_SP_ALL, region);


        /* Protect FBMEM - 8MB (Rushmore) */
        tzc_configure_region(region, CFG_FBMEM_START, 
                TZC_ATTR_SP_S_RW | TZC_ATTR_REGION_SIZE(TZC_REGION_SIZE_8M) | TZC_ATTR_REGION_ENABLE);
        region++;
        DMSG("Added CFG_FBMEM_START to TZC region");
  
        /* Protect IPU1 System Memory - 2MB (Rushmore) */
        tzc_configure_region(region, IPU_BASE, 
                TZC_ATTR_SP_S_RW | TZC_ATTR_REGION_SIZE(TZC_REGION_SIZE_2M) | TZC_ATTR_REGION_ENABLE);
        region++;
        DMSG("Added IPU_BASE to TZC region");

        /* Protect CAAM System Memory - 16KB, but 32KB due to the smallest size to configure (Rushmore) */
        tzc_configure_region(region, CAAM_BASE, 
                TZC_ATTR_SP_S_RW | TZC_ATTR_REGION_SIZE(TZC_REGION_SIZE_32K) | TZC_ATTR_REGION_ENABLE);
        region++;
        DMSG("Added CAAM_BASE to TZC region");




        /* Overlay Buffer Address */
//        int REG_OVERLAY_BUF_ADDR_SIZE = 29;
//        uint32_t REG_OVERLAY_BUF_ADDR = 
//            ipu_ch_param_addr(IPU_BASE + IPU_CPMEM_OFFSET, IPU_CHAN_27, 1, 29 * 0, 29);
    
//        region = tzc_auto_configure(REG_OVERLAY_BUF_ADDR, REG_OVERLAY_BUF_ADDR_SIZE, 
//                TZC_ATTR_SP_S_RW, region);
//        tzc_configure_region(region, REG_OVERLAY_BUF_ADDR, 
//                TZC_ATTR_SP_S_RW | TZC_ATTR_REGION_SIZE(TZC_REGION_SIZE_32K) | TZC_ATTR_REGION_ENABLE);
//        region++;

//        DMSG("[REG_OVERLAY_BUF_ADDR] added to TZC region");


        /* Overlay Channel Enable */
//        int REG_OVERLAY_ENABLE_SIZE = 1;
//        uint32_t REG_OVERLAY_ENABLE = IPU_BASE + IPU_CM_OFFSET + IPUx_IDMAC_CH_EN_1 + 27;

//        tzc_configure_region(region, REG_OVERLAY_ENABLE, 
//                TZC_ATTR_SP_S_RW | TZC_ATTR_REGION_SIZE(TZC_REGION_SIZE_32K) | TZC_ATTR_REGION_ENABLE);
//        region++;

//        DMSG("[REG_OVERLAY_ENABLE] added to TZC region");


        /* Partial Plane Configs */
//        int REG_OVERLAY_PARTIAL_SIZE = 32;
//        uint32_t REG_OVERLAY_PARTIAL = IPU_BASE + IPU_CM_OFFSET + IPUx_DP_COM_CONF_SYNC + 0;

//        tzc_configure_region(region, REG_OVERLAY_PARTIAL, 
//                TZC_ATTR_SP_S_RW | TZC_ATTR_REGION_SIZE(TZC_REGION_SIZE_32K) | TZC_ATTR_REGION_ENABLE);
//        region++;

//        DMSG("[REG_OVERLAY_PARTIAL] added to TZC region");


        /* Alpha Value of Overlay Channel */
//        int REG_OVERLAY_ALPHA_VAL_SIZE = 8;
//        uint32_t REG_OVERLAY_ALPHA_VAL = IPU_BASE + IPU_CM_OFFSET + IPUx_DP_Graph_Wind_CTRL_SYNC + 24;

        /* Skip because 32KB from above partial plane config includes this register */
//        tzc_configure_region(region, REG_OVERLAY_ALPHA, 
//                TZC_ATTR_SP_S_RW | TZC_ATTR_REGION_SIZE(TZC_REGION_SIZE_32K) | TZC_ATTR_REGION_ENABLE);
//        region++;

//        DMSG("[REG_OVERLAY_ALPHA_VAL] added to TZC region");

		if (tzc_regions_lockdown() != TEE_SUCCESS)
			panic("Region lockdown failed!");
	}



	return TEE_SUCCESS;
}
driver_init(imx_configure_tzasc);
