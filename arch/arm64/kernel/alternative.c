/*
 * alternative runtime patching
 * inspired by the x86 version
 *
 * Copyright (C) 2014 ARM Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define pr_fmt(fmt) "alternatives: " fmt

#include <linux/init.h>
#include <linux/cpu.h>
#include <asm/cacheflush.h>
#include <asm/alternative.h>
#include <asm/cpufeature.h>
#include <asm/insn.h>
#include <asm/sections.h>
#include <linux/stop_machine.h>

#define __ALT_PTR(a,f)		((void *)&(a)->f + (a)->f)
#define ALT_ORIG_PTR(a)		__ALT_PTR(a, orig_offset)
#define ALT_REPL_PTR(a)		__ALT_PTR(a, alt_offset)

struct alt_region {
	struct alt_instr *begin;
	struct alt_instr *end;
};

/*
 * Check if the given address needs a relocation fixup
 * in case the given alt-seq is moved.
 * This is required if the address is e.g. in kernel-text,
 * but not if it is within the given alt-seq itself.
 * We call BUG() in cases where we cannot safely decide.
 */
static bool address_needs_relocation_fixup(struct alt_instr *alt, unsigned long addr)
{
	unsigned long replptr;

	if (kernel_text_address(addr) || core_kernel_data(addr))
		return 1;

	replptr = (unsigned long)ALT_REPL_PTR(alt);
	if (addr >= replptr && addr <= (replptr + alt->alt_len))
		return 0;

	/*
	 * Branching into *another* alternate sequence is doomed, and
	 * we're not even trying to fix it up.
	 */
	BUG();
}

#define align_down(x, a)	((unsigned long)(x) & ~(((unsigned long)(a)) - 1))

static u32 get_alt_insn(struct alt_instr *alt, __le32 *insnptr, __le32 *altinsnptr)
{
	u32 insn;

	insn = le32_to_cpu(*altinsnptr);

	if (aarch64_insn_is_branch_imm(insn)) {
		s32 offset = aarch64_get_branch_offset(insn);
		unsigned long target;

		target = (unsigned long)altinsnptr + offset;

		/*
		 * If we're branching inside the alternate sequence,
		 * do not rewrite the instruction, as it is already
		 * correct. Otherwise, generate the new instruction.
		 */
		if (address_needs_relocation_fixup(alt, target)) {
			offset = target - (unsigned long)insnptr;
			insn = aarch64_set_branch_offset(insn, offset);
		}
	} else if (aarch64_insn_is_adrp(insn)) {
		s32 orig_offset, new_offset;
		unsigned long target;

		orig_offset  = aarch64_insn_adrp_get_offset(insn);
		target = align_down(altinsnptr, SZ_4K) + orig_offset;

		if (address_needs_relocation_fixup(alt, target)) {
			/*
			 * If we're replacing an adrp instruction, which uses
			 * PC-relative immediate addressing, adjust the offset
			 * to reflect the new PC. adrp operates on 4K aligned
			 * addresses.
			 */
			new_offset = target - align_down(insnptr, SZ_4K);
			insn = aarch64_insn_adrp_set_offset(insn, new_offset);
		}
	} else if (aarch64_insn_is_adr(insn)) {
		s32 offset = aarch64_insn_adr_get_offset(insn);
		unsigned long target;

		target = (unsigned long)altinsnptr + offset;

		if (address_needs_relocation_fixup(alt, target)) {
			/*
			 * Disallow adr instructions for targets outside
			 * of our alt block.
			 */
			BUG();
		}
	} else if (aarch64_insn_uses_literal(insn)) {
		/*
		 * Disallow patching unhandled instructions using PC relative
		 * literal addresses
		 */
		BUG();
	}

	return insn;
}

static void __apply_alternatives(void *alt_region, bool use_linear_alias)
{
	struct alt_instr *alt;
	struct alt_region *region = alt_region;
	__le32 *origptr, *replptr, *updptr;

	for (alt = region->begin; alt < region->end; alt++) {
		u32 insn;
		int i, nr_inst;

		if (!cpus_have_cap(alt->cpufeature))
			continue;

		BUG_ON(alt->alt_len != alt->orig_len);

		pr_info_once("patching kernel code\n");

		origptr = ALT_ORIG_PTR(alt);
		replptr = ALT_REPL_PTR(alt);
		updptr = use_linear_alias ? lm_alias(origptr) : origptr;
		nr_inst = alt->alt_len / sizeof(insn);

		for (i = 0; i < nr_inst; i++) {
			insn = get_alt_insn(alt, origptr + i, replptr + i);
			updptr[i] = cpu_to_le32(insn);
		}

		flush_icache_range((uintptr_t)origptr,
				   (uintptr_t)(origptr + nr_inst));
	}
}

/*
 * We might be patching the stop_machine state machine, so implement a
 * really simple polling protocol here.
 */
static int __apply_alternatives_multi_stop(void *unused)
{
	static int patched = 0;
	struct alt_region region = {
		.begin	= (struct alt_instr *)__alt_instructions,
		.end	= (struct alt_instr *)__alt_instructions_end,
	};

	/* We always have a CPU 0 at this point (__init) */
	if (smp_processor_id()) {
		while (!READ_ONCE(patched))
			cpu_relax();
		isb();
	} else {
		BUG_ON(patched);
		__apply_alternatives(&region, true);
		/* Barriers provided by the cache flushing */
		WRITE_ONCE(patched, 1);
	}

	return 0;
}

void __init apply_alternatives_all(void)
{
	/* better not try code patching on a live SMP system */
	stop_machine(__apply_alternatives_multi_stop, NULL, cpu_online_mask);
}

void apply_alternatives(void *start, size_t length)
{
	struct alt_region region = {
		.begin	= start,
		.end	= start + length,
	};

	__apply_alternatives(&region, false);
}
