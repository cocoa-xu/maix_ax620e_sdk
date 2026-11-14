// SPDX-License-Identifier: GPL-2.0+
/*
 * axwdt <seconds>: start watchdog 0 (reset mode) for the kernel to take over.
 * The SD boot path leaves it off, so a kernel that hangs early would never
 * come back to U-Boot and its boot counter.
 */

#include <common.h>
#include <command.h>

void set_wdt0_timeout(u32 time);

static int do_axwdt(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	ulong seconds;

	if (argc != 2)
		return CMD_RET_USAGE;

	seconds = simple_strtoul(argv[1], NULL, 10);
	if (!seconds)
		return CMD_RET_USAGE;

	set_wdt0_timeout(seconds);

	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(axwdt, 2, 0, do_axwdt, "start watchdog 0", "<seconds>");
