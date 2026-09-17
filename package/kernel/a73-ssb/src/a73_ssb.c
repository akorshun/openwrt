// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Cortex-A73 speculative store bypass chicken bit.
 *
 * ARM Trusted Firmware applies the CVE-2018-3639 mitigation on Cortex-A73 in
 * its reset handler, on every core: it sets bit 3, "disable load pass store",
 * of the implementation defined register S3_0_C15_C0_0. Loads can then no
 * longer be issued ahead of stores whose address is not resolved yet, which
 * costs about a factor of two in integer code that works through memory.
 *
 * Linux cannot see it. The firmware answers SMCCC_ARCH_WORKAROUND_2 with
 * "not required" because it has mitigated the core permanently, so the kernel
 * reports spec_store_bypass as "Not affected" even though the core is
 * affected and mitigated in the most expensive way.
 *
 * This module reports the register on every CPU, and with clear=1 also clears
 * the bit. Whether the access works at all is up to ACTLR_EL3.CPUACTLR, which
 * only EL3 can set: if the firmware does not grant it, the instruction traps
 * to EL3 rather than to us, and the machine most likely hangs. Loading this
 * module is a test, not a supported configuration; clearing the bit gives up
 * the Spectre v4 mitigation.
 */

#define pr_fmt(fmt) "a73-ssb: " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/smp.h>
#include <asm/barrier.h>
#include <asm/cputype.h>
#include <asm/sysreg.h>

#define A73_IMP_DEF_REG1		sys_reg(3, 0, 15, 0, 0)
#define A73_DISABLE_LOAD_PASS_STORE	BIT(3)

static bool clear;
module_param(clear, bool, 0444);
MODULE_PARM_DESC(clear,
		 "clear the disable-load-pass-store bit, giving up the Spectre v4 mitigation");

static void a73_ssb_cpu(void *unused)
{
	u64 val = read_sysreg_s(A73_IMP_DEF_REG1);

	pr_info("cpu%d: S3_0_C15_C0_0 = 0x%016llx, load pass store %s\n",
		smp_processor_id(), val,
		(val & A73_DISABLE_LOAD_PASS_STORE) ? "disabled" : "enabled");

	if (!clear || !(val & A73_DISABLE_LOAD_PASS_STORE))
		return;

	write_sysreg_s(val & ~A73_DISABLE_LOAD_PASS_STORE, A73_IMP_DEF_REG1);
	isb();

	val = read_sysreg_s(A73_IMP_DEF_REG1);
	pr_info("cpu%d: now 0x%016llx, load pass store %s\n",
		smp_processor_id(), val,
		(val & A73_DISABLE_LOAD_PASS_STORE) ? "disabled" : "enabled");
}

static int __init a73_ssb_init(void)
{
	u32 midr = read_cpuid_id();

	if (MIDR_IMPLEMENTOR(midr) != ARM_CPU_IMP_ARM ||
	    MIDR_PARTNUM(midr) != ARM_CPU_PART_CORTEX_A73) {
		pr_err("not a Cortex-A73 (MIDR 0x%08x)\n", midr);
		return -ENODEV;
	}

	on_each_cpu(a73_ssb_cpu, NULL, 1);

	return 0;
}

static void __exit a73_ssb_exit(void)
{
}

module_init(a73_ssb_init);
module_exit(a73_ssb_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Report or clear the Cortex-A73 disable-load-pass-store bit");
