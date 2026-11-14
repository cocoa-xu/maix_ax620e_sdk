// SPDX-License-Identifier: GPL-2.0+
/*
 * The environment of the A/B boot lives at fixed offsets ahead of the first
 * SD partition. The vendor mmc_offset() would place the first copy in an
 * "env" partition of its default (eMMC) layout, 0x440000, which on the SD
 * card is inside the FAT boot partition.
 */

#include <common.h>
#include <mmc.h>

int mmc_get_env_addr(struct mmc *mmc, int copy, u32 *env_addr)
{
	*env_addr = copy ? CONFIG_ENV_OFFSET_REDUND : CONFIG_ENV_OFFSET;

	return 0;
}
