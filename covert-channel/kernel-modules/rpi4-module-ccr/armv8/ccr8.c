// Original Source:
//   https://matthewarcus.wordpress.com/2018/01/27/using-the-cycle-counter-registers-on-the-raspberry-pi-3/
// Information on Cortex-A72 registers:
//   https://developer.arm.com/documentation/100095/0002/system-control/aarch64-register-summary/aarch64-performance-monitors-registers
//   https://developer.arm.com/docs/ddi0595/h/aarch64-system-registers/pmuserenr_el0
#include <linux/module.h>
#include <linux/kernel.h>

#define ARMV8_PMUSERENR_EN (1<<0)
#define ARMV8_PMUSERENR_CR (1<<2)
#define ARMV8_PMUSERENR_ER (1<<3)

void enable_ccr(void *info) {
	// Allow enabling the cycle count register from user mode:
	// Set bit 0 (EN), 2 (CR) and 3 (ER) in PMUSERENR_EL0
	asm volatile("MSR PMUSERENR_EL0, %0" : : "r"((u64)ARMV8_PMUSERENR_EN|ARMV8_PMUSERENR_CR|ARMV8_PMUSERENR_ER));
}

void disable_ccr(void *info) {
	// Disallow enabling the cycle count register from user mode
	// Set PMUSERENR_EL0 to 0
	asm volatile("MSR PMUSERENR_EL0, %0" : : "r"((u64)0));
}

int init_module(void) {
	// Each cpu has its own set of registers
	on_each_cpu(enable_ccr, NULL, 0);
	printk(KERN_INFO "Userspace access to CCR enabled\n");
	return 0;
}
 
void cleanup_module(void) {
	// Each cpu has its own set of registers
	on_each_cpu(disable_ccr, NULL, 0);
	printk(KERN_INFO "Userspace access to CCR disabled\n");
}

MODULE_LICENSE("GPL");  // avoid "taints kernel" message
