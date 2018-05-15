/* SPDX-License-Identifier: GPL-2.0 */
#include <asm-generic/asm-prototypes.h>

#ifdef CONFIG_RETPOLINE
#define INDIRECT_THUNK(reg) \
extern asmlinkage void __aarch64_indirect_thunk_ ## reg(void);

INDIRECT_THUNK(x0);
INDIRECT_THUNK(x1);
INDIRECT_THUNK(x2);
INDIRECT_THUNK(x3);
INDIRECT_THUNK(x4);
INDIRECT_THUNK(x5);
INDIRECT_THUNK(x6);
INDIRECT_THUNK(x7);
INDIRECT_THUNK(x8);
INDIRECT_THUNK(x9);
INDIRECT_THUNK(x10);
INDIRECT_THUNK(x11);
INDIRECT_THUNK(x12);
INDIRECT_THUNK(x13);
INDIRECT_THUNK(x14);
INDIRECT_THUNK(x15);
INDIRECT_THUNK(x16);
INDIRECT_THUNK(x17);
INDIRECT_THUNK(x18);
INDIRECT_THUNK(x19);
INDIRECT_THUNK(x20);
INDIRECT_THUNK(x21);
INDIRECT_THUNK(x22);
INDIRECT_THUNK(x23);
INDIRECT_THUNK(x24);
INDIRECT_THUNK(x25);
INDIRECT_THUNK(x26);
INDIRECT_THUNK(x27);
INDIRECT_THUNK(x28);
INDIRECT_THUNK(x29);
INDIRECT_THUNK(x30);

#endif /* CONFIG_RETPOLINE */
