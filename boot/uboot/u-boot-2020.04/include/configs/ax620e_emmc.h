/*
 * (C) Copyright 2020 AXERA Co., Ltd
 *
 * SPDX-License-Identifier:     GPL-2.0+
 */

#ifndef __AX620E_EMMC_H
#define __AX620E_EMMC_H

#include <configs/ax620e_common.h>

#define COUNTER_FREQUENCY    24000000
#define MEM_REGION_DDR_SIZE (0x100000000UL)

/* boot-logo related buffer address */
#define LOGO_IMAGE_LOAD_ADDR 0x70000000U
#define LOGO_SHOW_BUFFER 0x71000000U
#define LOGO_SHOW_BUF_SIZE 0x800000U

#define CONFIG_SYS_MALLOC_LEN		(64 << 20)	//64M

#define SDRAM_BANK_SIZE			(2UL << 30)

#ifdef CONFIG_AXERA_AX630C_DDR4_RETRAIN
#define SDHCI_ALIGN_BUF_ADDR	0x40001000
#else
#define SDHCI_ALIGN_BUF_ADDR	0x40000000
#endif
#define SDHCI_ALIGN_BUF_BLKS	0x1000
#define FDL_BUF_ADDR        	(SDHCI_ALIGN_BUF_ADDR + 0x1000)
#define FDL_BUF_LEN         	0x2000000
#define SD_UPDATE_BUF       	(SDHCI_ALIGN_BUF_ADDR + 0x2001000)
#define USB_UPDATE_BUF      	(SDHCI_ALIGN_BUF_ADDR + 0x2001000)
#ifdef CONFIG_AX620Q_EMMC
#define SPARSE_IMAGE_BUF_ADDR	(0x40000000 + 4 * 1024 * 1024)
#define SPARSE_IMAGE_BUF_LEN	0x9000000
#define OTA_BUF_ADDR        	(UBOOT_IMG_HEADER_BASE + 4 * 1024 * 1024)
#define OTA_BUF_LEN         	0x9000000
#else
#define SPARSE_IMAGE_BUF_ADDR	(UBOOT_IMG_HEADER_BASE + 4 * 1024 * 1024)
#define SPARSE_IMAGE_BUF_LEN	0x10000000
#define OTA_BUF_ADDR        	(UBOOT_IMG_HEADER_BASE + 4 * 1024 * 1024)
#define OTA_BUF_LEN         	0x40000000
#endif
#define CONFIG_SYS_WHITE_ON_BLACK

#define EMMC_DEV_ID          0
#define SD_DEV_ID            1

#ifdef CONFIG_AXERA_NANOKVM_AB
/*
 * Nerves A/B boot from the SD card (layout in nerves_system_nano_kvm_go's
 * fwup.conf): a FIT image (zImage + DTB, sha256) in a raw slot per side,
 * rootfs A/B on p2/p3. nerves_fw_booted/validated work as in other Nerves
 * systems: an update that reboots without being validated (by the
 * application, nerves_fw_autovalidate=0) reverts. A slot
 * that fails to load reverts at once. The BOOT button boots the other slot.
 * The FIT is loaded at fit_addr_r and its kernel copied to 0x42000000: the
 * zImage has to run in the first 128 MiB of DRAM, where it derives the
 * kernel's physical base. sdupdate=finish keeps the vendor SD update mode
 * (and its watchdog stop) out.
 */
#define CONFIG_EXTRA_ENV_SETTINGS \
	"sdupdate=finish\0" \
	"nerves_fw_active=a\0" \
	"nerves_fw_autovalidate=0\0" \
	"nerves_fw_validated=1\0" \
	"nerves_fw_booted=0\0" \
	"kernel_a_blk=0x42000\0" \
	"kernel_b_blk=0x52000\0" \
	"kernel_blks=0x8000\0" \
	"fit_addr_r=0x45000000\0" \
	"cmdline=\0" \
	"nerves_revert=" \
		"if test \"${nerves_fw_active}\" = a; then " \
		"setenv nerves_fw_active b; else setenv nerves_fw_active a; fi\0" \
	"nerves_init=" \
		"if test \"${nerves_fw_booted}\" = 1; then " \
		"if test \"${nerves_fw_validated}\" = 0; then " \
		"run nerves_revert; setenv nerves_fw_validated 1; saveenv; fi; " \
		"else setenv nerves_fw_booted 1; " \
		"if test \"${nerves_fw_autovalidate}\" = 1; then " \
		"setenv nerves_fw_validated 1; fi; saveenv; fi\0" \
	"nerves_load=" \
		"if test \"${nerves_fw_active}\" = b; then " \
		"setenv kernel_blk ${kernel_b_blk}; setenv rootfs_part 3; " \
		"else setenv kernel_blk ${kernel_a_blk}; setenv rootfs_part 2; fi; " \
		"mmc dev 1 && mmc read ${fit_addr_r} ${kernel_blk} ${kernel_blks}\0" \
	"nerves_try=" \
		"run nerves_load && setenv bootargs console=ttyS3,115200n8 " \
		"earlycon=uart8250,mmio32,0x6080000 " \
		"root=/dev/mmcblk0p${rootfs_part} rootfstype=squashfs rootwait ro " \
		"${cmdline} && axwdt 120 && bootm ${fit_addr_r}\0" \
	"nerves_bootcmd=" \
		"run nerves_init; run nerves_try; run nerves_revert; " \
		"setenv nerves_fw_validated 1; saveenv; run nerves_try\0" \
	"nerves_bootkey=" \
		"run nerves_revert; setenv nerves_fw_validated 1; saveenv; " \
		"run nerves_try\0"
#endif

#endif
