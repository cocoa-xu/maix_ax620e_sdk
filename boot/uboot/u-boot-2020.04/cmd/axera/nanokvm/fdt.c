// SPDX-License-Identifier: GPL-2.0+
/*
 * Pass the chip UID, which SPL copies from the eFuses into IRAM, to Linux as
 * the device tree's serial-number (where Nerves' boardid looks for it).
 */

#include <common.h>
#include <fdt_support.h>
#include <asm/arch/boot_mode.h>

int ft_board_setup(void *blob, bd_t *bd)
{
	misc_info_t *info = (misc_info_t *)MISC_INFO_ADDR;
	char serial[17];

	snprintf(serial, sizeof(serial), "%08x%08x", info->uid_h, info->uid_l);

	return fdt_setprop_string(blob, 0, "serial-number", serial);
}
