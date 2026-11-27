// SPDX-License-Identifier: GPL-2.0+
/*
 * go64 <bl31> <kernel> <fdt>: boots AArch64 Linux from this AArch32 U-Boot,
 * through TF-A's BL31.
 *
 * SPL leaves AArch64 for U-Boot by a warm reset and enters it at EL3, so
 * U-Boot may warm-reset the cores back (RMR): each restarts in AArch64 at
 * the address in its RVBAR. Core 0 then enters BL31 with the parameters the
 * arm64 SPL gives it; core 1 leaves SPL's AArch32 pen for a pen in AArch64,
 * where BL31's PSCI CPU_ON finds it. The device tree gets the fixups bootm
 * gives one: the boot arguments, the DRAM, the CMA pools.
 */

#include <common.h>
#include <command.h>
#include <cpu_func.h>
#include <fdt_support.h>
#include <image.h>
#include <time.h>
#include <linux/libfdt.h>
#include <asm/io.h>

#define RVBAR0_H		0x02330004
#define RVBAR0_L		0x02330008
#define RVBAR1_H		0x0233000c
#define RVBAR1_L		0x02330010
/* Core 1's entry: SPL's pen waits for it, BL31's CPU_ON sets it. */
#define CORE1_ENTRY_L		0x023400dc
#define CORE1_ENTRY_H		0x023400e0

/* The last page of the 256 KiB the device tree reserves for BL31. */
#define STUBS			0x4007f000
#define CORE1_A32		(STUBS + 0x0c0)
#define CORE0_A64		(STUBS + 0x100)
#define CORE1_A64		(STUBS + 0x200)
#define PARAMS			(STUBS + 0x400)

#define COUNTER_HZ		24000000
#define FDT_PAD			0x4000

/* TF-A's parameters for BL31 */
#define PARAM_EP		0x01
#define PARAM_BL_PARAMS		0x05
#define BL33_IMAGE_ID		5
#define EP_NON_SECURE		0x1
#define SPSR_EL1H_DAIF		0x3c5
#define ARM_BL31_PLAT_PARAM_VAL	0x0f1e2d3c4b5a6978ULL

struct tfa_header {
	u8 type;
	u8 version;
	u16 size;
	u32 attr;
};

struct tfa_ep_info {
	struct tfa_header h;
	u64 pc;
	u32 spsr;
	u32 pad;
	u64 args[8];
};

struct tfa_params_node {
	u32 image_id;
	u32 pad;
	u64 image_info;
	u64 ep_info;
	u64 next;
};

/* bl_params_t, followed by its one node: BL33 */
struct tfa_params {
	struct tfa_header h;
	u64 head;
	struct tfa_params_node bl33;
	struct tfa_ep_info bl33_ep;
};

/* Core 1, sent from SPL's pen: clears its entry, warm-resets into AArch64. */
static const u32 core1_a32[] = {
	0xe59f0020,	/*	ldr	r0, 1f				*/
	0xe3a01000,	/*	mov	r1, #0				*/
	0xe5801000,	/*	str	r1, [r0]			*/
	0xf57ff04f,	/*	dsb	sy				*/
	0xf57ff06f,	/*	isb	sy				*/
	0xe3a00003,	/*	mov	r0, #3		@ RMR: AA64, RR	*/
	0xee0c0f50,	/*	mcr	p15, 0, r0, c12, c0, 2		*/
	0xf57ff06f,	/*	isb	sy				*/
	0xe320f003,	/* 0:	wfi					*/
	0xeafffffd,	/*	b	0b				*/
	CORE1_ENTRY_L,	/* 1:						*/
};

/* Core 0 at its RVBAR: the counter's frequency, then BL31. */
struct core0_a64 {
	u32 code[8];
	u64 cntfrq, params, fdt, plat_param, bl31;
};

static const u32 core0_code[] = {
	0x58000100,	/*	ldr	x0, cntfrq			*/
	0xd51be000,	/*	msr	cntfrq_el0, x0			*/
	0x58000100,	/*	ldr	x0, params			*/
	0xd2800001,	/*	mov	x1, #0				*/
	0x58000102,	/*	ldr	x2, fdt				*/
	0x58000123,	/*	ldr	x3, plat_param			*/
	0x58000144,	/*	ldr	x4, bl31			*/
	0xd61f0080,	/*	br	x4				*/
};

/* Core 1 at its RVBAR: waits for an entry from PSCI's CPU_ON. */
struct core1_a64 {
	u32 code[10];
	u64 cntfrq, entry_l, entry_h;
};

static const u32 core1_code[] = {
	0x58000140,	/*	ldr	x0, cntfrq			*/
	0xd51be000,	/*	msr	cntfrq_el0, x0			*/
	0x58000141,	/*	ldr	x1, entry_l			*/
	0x58000162,	/*	ldr	x2, entry_h			*/
	0xd503205f,	/* 0:	wfe					*/
	0xb9400023,	/*	ldr	w3, [x1]			*/
	0xb9400044,	/*	ldr	w4, [x2]			*/
	0xaa048065,	/*	orr	x5, x3, x4, lsl #32		*/
	0xb4ffff85,	/*	cbz	x5, 0b				*/
	0xd61f00a0,	/*	br	x5				*/
};

static void write_stubs(ulong bl31, ulong kernel, ulong fdt)
{
	struct core0_a64 *core0 = (void *)CORE0_A64;
	struct core1_a64 *core1 = (void *)CORE1_A64;
	struct tfa_params *params = (void *)PARAMS;

	memcpy((void *)CORE1_A32, core1_a32, sizeof(core1_a32));

	memcpy(core0->code, core0_code, sizeof(core0_code));
	core0->cntfrq = COUNTER_HZ;
	core0->params = PARAMS;
	core0->fdt = fdt;
	core0->plat_param = ARM_BL31_PLAT_PARAM_VAL;
	core0->bl31 = bl31;

	memcpy(core1->code, core1_code, sizeof(core1_code));
	core1->cntfrq = COUNTER_HZ;
	core1->entry_l = CORE1_ENTRY_L;
	core1->entry_h = CORE1_ENTRY_H;

	memset(params, 0, sizeof(*params));
	params->h = (struct tfa_header){ PARAM_BL_PARAMS, 2, 16, 0 };
	params->head = PARAMS + offsetof(struct tfa_params, bl33);
	params->bl33.image_id = BL33_IMAGE_ID;
	params->bl33.ep_info = PARAMS + offsetof(struct tfa_params, bl33_ep);
	params->bl33_ep.h = (struct tfa_header){
		PARAM_EP, 1, sizeof(struct tfa_ep_info), EP_NON_SECURE
	};
	params->bl33_ep.pc = kernel;
	params->bl33_ep.spsr = SPSR_EL1H_DAIF;
	params->bl33_ep.args[0] = fdt;
}

/* Sends core 1 from SPL's pen to its stub; true once it has left. */
static bool release_core1(void)
{
	ulong start = get_timer(0);

	writel(0, CORE1_ENTRY_H);
	writel(CORE1_A32, CORE1_ENTRY_L);
	asm volatile("dsb sy\n\tsev" : : : "memory");

	while (readl(CORE1_ENTRY_L))
		if (get_timer(start) > 1000)
			return false;

	return true;
}

static int do_go64(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	bootm_headers_t images = { 0 };
	ulong bl31, kernel;
	void *fdt;

	if (argc != 4)
		return CMD_RET_USAGE;

	bl31 = simple_strtoul(argv[1], NULL, 16);
	kernel = simple_strtoul(argv[2], NULL, 16);
	fdt = (void *)simple_strtoul(argv[3], NULL, 16);

	if (fdt_check_header(fdt) ||
	    fdt_open_into(fdt, fdt, fdt_totalsize(fdt) + FDT_PAD) ||
	    image_setup_libfdt(&images, fdt, fdt_totalsize(fdt), NULL)) {
		printf("go64: no device tree at %p\n", fdt);
		return CMD_RET_FAILURE;
	}

	write_stubs(bl31, kernel, (ulong)fdt);
	writel(0, RVBAR0_H);
	writel(CORE0_A64, RVBAR0_L);
	writel(0, RVBAR1_H);
	writel(CORE1_A64, RVBAR1_L);

	printf("go64: BL31 at 0x%lx, Linux at 0x%lx, device tree at %p\n",
	       bl31, kernel, fdt);
	cleanup_before_linux();
	if (!release_core1())
		printf("go64: core 1 stays in SPL\n");

	asm volatile("dsb sy\n\t"
		     "isb\n\t"
		     "mcr p15, 0, %0, c12, c0, 2\n\t"
		     "isb" : : "r" (3) : "memory");
	for (;;)
		asm volatile("wfi");
}

U_BOOT_CMD(go64, 4, 0, do_go64, "boot AArch64 Linux through TF-A's BL31",
	   "<bl31> <kernel> <fdt>");
