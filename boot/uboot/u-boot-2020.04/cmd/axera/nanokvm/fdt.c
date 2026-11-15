// SPDX-License-Identifier: GPL-2.0+
/*
 * Device tree fixups for Linux:
 * - the chip UID, which SPL copies from the eFuses into IRAM, as the
 *   serial-number (where Nerves' boardid looks for it);
 * - the sizes of the CMA pools of video and of the NPU, from the environment.
 */

#include <common.h>
#include <env.h>
#include <fdt_support.h>
#include <linux/libfdt.h>
#include <asm/arch/boot_mode.h>

/* Linux aligns CMA pools to page blocks: 2^11 pages of 4 KiB (MAX_ORDER 11) */
#define CMA_ALIGN_MB	8

/* The DRAM in the package in MiB, 0 for chips of other boards */
static unsigned long dram_mb(void)
{
	misc_info_t *info = (misc_info_t *)MISC_INFO_ADDR;

	switch (info->chip_type) {
	case AX620QE_CHIP:
		return 512;
	case AX620QF_CHIP:
		return 256;
	default:
		return 0;
	}
}

/* Linux's default CMA pool, the one video uses */
static int video_pool(const void *blob)
{
	int parent = fdt_path_offset(blob, "/reserved-memory");
	int node;

	if (parent < 0)
		return parent;

	fdt_for_each_subnode(node, blob, parent)
		if (fdt_getprop(blob, node, "linux,cma-default", NULL))
			return node;

	return -FDT_ERR_NOTFOUND;
}

static int npu_device(const void *blob)
{
	return fdt_node_offset_by_compatible(blob, -1, "axera,ax620e-npu");
}

static int npu_pool(const void *blob)
{
	int npu = npu_device(blob);
	const fdt32_t *region;

	if (npu < 0)
		return npu;

	region = fdt_getprop(blob, npu, "memory-region", NULL);
	if (!region)
		return -FDT_ERR_NOTFOUND;

	return fdt_node_offset_by_phandle(blob, fdt32_to_cpu(*region));
}

/* The size in MiB of a pool that Linux places itself, else 0 */
static unsigned long pool_mb(const void *blob, int node)
{
	const fdt32_t *size;
	int len;

	if (node < 0 || fdt_getprop(blob, node, "reg", NULL))
		return 0;

	size = fdt_getprop(blob, node, "size", &len);
	if (size && len == sizeof(*size))
		return fdt32_to_cpu(size[0]) >> 20;
	if (size && len == 2 * sizeof(*size) && !size[0])
		return fdt32_to_cpu(size[1]) >> 20;

	return 0;
}

static int set_pool_mb(void *blob, int node, unsigned long mb)
{
	int len;

	if (!fdt_getprop(blob, node, "size", &len))
		return len;
	if (len == 2 * sizeof(fdt32_t))
		return fdt_setprop_u64(blob, node, "size", (u64)mb << 20);

	return fdt_setprop_u32(blob, node, "size", mb << 20);
}

/*
 * A pool size from the environment in MiB, a multiple of CMA_ALIGN_MB from
 * min to max; anything else keeps the device tree's size.
 */
static unsigned long env_mb(const char *name, unsigned long dt_mb,
			    unsigned long min, unsigned long max)
{
	const char *value = env_get(name);
	unsigned long mb;
	char *end;

	if (!value)
		return dt_mb;

	mb = simple_strtoul(value, &end, 10);
	if (end == value || *end || strlen(value) > 5 || mb < min || mb > max ||
	    mb % CMA_ALIGN_MB) {
		printf("CMA: %s=%s is not a multiple of %d MiB from %lu to %lu, keeping %lu MiB\n",
		       name, value, CMA_ALIGN_MB, min, max, dt_mb);
		return dt_mb;
	}

	return mb;
}

/*
 * cma_video and cma_npu size the CMA pools of video (Linux's default pool)
 * and of the NPU in MiB: multiples of 8, video at least 8, the NPU 0 to turn
 * it off, together at most half of the DRAM. Sizes that are unset or out of
 * these bounds keep the device tree's.
 */
static void fdt_cma(void *blob)
{
	unsigned long half = dram_mb() / 2;
	unsigned long video_dt, npu_dt, video, npu;
	int err = 0;

	if (!env_get("cma_video") && !env_get("cma_npu"))
		return;

	video_dt = pool_mb(blob, video_pool(blob));
	npu_dt = pool_mb(blob, npu_pool(blob));
	if (!half || !video_dt) {
		printf("CMA: unknown DRAM size or video pool, keeping the device tree's sizes\n");
		return;
	}

	video = env_mb("cma_video", video_dt, CMA_ALIGN_MB, half);
	npu = npu_dt ? env_mb("cma_npu", npu_dt, 0, half) : 0;
	if (video + npu > half) {
		printf("CMA: %lu + %lu MiB is more than half of the DRAM, keeping %lu + %lu MiB\n",
		       video, npu, video_dt, npu_dt);
		return;
	}

	if (video != video_dt)
		err = set_pool_mb(blob, video_pool(blob), video);
	if (!err && npu && npu != npu_dt)
		err = set_pool_mb(blob, npu_pool(blob), npu);
	if (!err && !npu && npu_dt) {
		/* Its memory goes back to Linux and its driver does not bind */
		err = fdt_status_disabled(blob, npu_pool(blob));
		if (!err)
			err = fdt_status_disabled(blob, npu_device(blob));
	}

	if (err)
		printf("CMA: device tree update failed: %s\n", fdt_strerror(err));
	else
		printf("CMA: video %lu MiB, NPU %lu MiB\n", video, npu);
}

int ft_board_setup(void *blob, bd_t *bd)
{
	misc_info_t *info = (misc_info_t *)MISC_INFO_ADDR;
	char serial[17];
	int err;

	snprintf(serial, sizeof(serial), "%08x%08x", info->uid_h, info->uid_l);
	err = fdt_setprop_string(blob, 0, "serial-number", serial);
	fdt_cma(blob);

	return err;
}
