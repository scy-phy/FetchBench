// Original Source:
//   https://matthewarcus.wordpress.com/2018/01/27/using-the-cycle-counter-registers-on-the-raspberry-pi-3/
// Information on Cortex-A72 registers:
//   https://developer.arm.com/documentation/100095/0002/system-control/aarch32-register-summary/c9-registers
#include <linux/module.h>
#include <linux/kernel.h>
 
void enable_ccr(void *info) {
	// Allow enabling the cycle count register from user mode:
	// Set bit 0 to 1 in PMUSERENR
	asm volatile("MCR p15, 0, %0, c9, c14, 0" :: "r" (1));
}

void disable_ccr(void *info) {
	// Disallow enabling the cycle count register from user mode
	// Set bit 0 to 0 in PMUSERENR
	asm volatile("MCR p15, 0, %0, c9, c14, 0" :: "r" (0));
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