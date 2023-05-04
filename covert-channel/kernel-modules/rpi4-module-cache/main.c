// ioctl parts based on https://tldp.org/LDP/lkmpg/2.4/html/x856.html

#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/uaccess.h> 
#include <linux/pid.h>
#include <linux/version.h>
#include <linux/sched/task.h>

#include <asm/page.h>
#include <asm/io.h>

#include "cache.h" 				// kernel functions to read the cache
#include "cache_structures.h" 	// cache_line, constants
#include "cache_ioctl.h"		// ioctl-related definitions

// The name for the device, as it will appear in /proc/devices
#define DEVICE_NAME DEVICE_FILE_NAME

// Is the device open right now? Used to prevent concurent access into the same device.
static int device_open_ctr = 0;

// setup data structure
struct cache_request_setup setup = { 0 };

// data structures for the address translation table for the caller's probe array
uint64_t* translation_table = NULL;
size_t no_translation_table_entries = 0;
#define MAX_TRANSLATION_TABLE_ENTRIES (4096)

// data structures to store cache hits when they are detected (before copying them to userspace)
size_t* l1_hits;
size_t* l2_hits;

// ioctl functions ***************************

/**
 * @brief      Called whenever a process attempts to open the device file.
 *             allow only 1 process to open the device at a time
 */
static int device_open(struct inode *inode, struct file *file) {
	if (device_open_ctr != 0) {
		return -EBUSY;
	}
	device_open_ctr++;
	return 0;
}

/**
 * @brief      Called when a process closes the device file.
 *
 */
static int device_release(struct inode *inode, struct file *file) {
	device_open_ctr--;

	// some cleanup
	setup = (struct cache_request_setup) { 0 };
	no_translation_table_entries = 0;
	
	return 0;
}

/**
 * @brief      Perform a page table walk to find the physical address of a
 *             virtual address from userspace.
 *
 * @param      mm                 The memory map of the userspace process
 * @param      userspace_virtual  The userspace virtual address to look up
 *
 * @return     physical address
 */
uint64_t translate_virt_to_phys_addr(struct mm_struct* mm, uint8_t* userspace_virtual) {
	uint64_t result_phys_addr = 0;
	unsigned long addr = (unsigned long) userspace_virtual;
	pgd_t *pgd;
	#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,11,0))
		p4d_t *p4d;
	#endif
	pud_t *pud;
	pmd_t *pmd;
	pte_t *ptep, pte;

	struct page *page = NULL;

	// walk through the levels of the page table
	pgd = pgd_offset(mm, addr);
	if (pgd_none(*pgd) || pgd_bad(*pgd)) goto err;

	#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,11,0))
		p4d = p4d_offset(pgd, addr);
		if (p4d_none(*p4d) || p4d_bad(*p4d)) goto err;
	#endif

	#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,11,0))
		pud = pud_offset(p4d, addr);
	#else
		pud = pud_offset(pgd, addr);
	#endif
	if (pud_none(*pud) || pud_bad(*pud)) goto err;

	pmd = pmd_offset(pud, addr);
	if (pmd_none(*pmd) || pmd_bad(*pmd)) goto err;

	ptep = pte_offset_map(pmd, addr);
	if (!ptep) {
		goto err;
	}
	pte = *ptep;
	page = pte_page(pte);
	if (!page) goto err;

	// found the pte for the page!! now extract the corresponding physical address
	result_phys_addr = page_to_phys(page);
	pte_unmap(ptep);
	return result_phys_addr;
err:
	printk(KERN_INFO "translation failed.");
	return 0;
}

/**
 * @brief      Helper function to run the query of L1 cache. Each CPU has
 *             its own L1 cache, so it's important to query the right one.
 *             For now, I will just always query the L1 of CPU3. The kernel
 *             does not provide a way to execute code on a given CPU (at
 *             least I did not find one). To work around this, this
 *             function is called on all CPUs, and the cache query is only
 *             executed when the function happens to run on the right one.
 *
 * @param      info  pointer to a size_t variable where the number of
 *                   results can be stored after successful execution.
 */
void query_l1(void* info) {
	// get number of current cpu and disable preemption -> we stay on this
	// CPU until we release it with put_cpu().
	int cpuno = get_cpu();
	if (cpuno == 3){
		// run query (only on CPU 3)
		size_t* num_results_l1 = (size_t*)info;
		*num_results_l1 = run_query_l1(translation_table, no_translation_table_entries, l1_hits);
	}
	put_cpu();
}


/**
 * @brief      Called whenever a process tries to do an ioctl on our device file.
 *
 * @param      file         The device file
 * @param[in]  ioctl_num    The ioctl number called
 * @param[in]  ioctl_param  The ioctl parameter given to the ioctl function
 *
 * @return     0 on success, -1 otherwise
 */
long device_ioctl(struct file *file, unsigned int ioctl_num, unsigned long ioctl_param) {
	unsigned long res;

	switch (ioctl_num) {
		// handle setup request. a setup request prepares a later cache query.
		case IOCTL_SETUP: {
			struct pid* caller_pid;
			struct task_struct* caller_task;
			struct mm_struct* caller_mm;

			// ioctl parameter is pointer to setup struct.
			// get that pointer and copy the struct to kernel space
			struct cache_request_setup* setup_ptr = (struct cache_request_setup*) ioctl_param;
			res = copy_from_user(&setup, setup_ptr, sizeof(struct cache_request_setup));
			if (res != 0) {
				printk(KERN_WARNING "could not copy setup struct from user space\n");
				return -1;
			}
			
			// process the setup request
			// 1. get the memory map of the calling userspace process
			caller_pid = find_get_pid(setup.pid);
			// get_pid_task increments a usage counter in the task struct.
			// needs to be decremented later by calling put_task_struct,
			// otherwise the kernel will keep the struct forever, even when
			// the user space process exits.
			caller_task = get_pid_task(caller_pid, PIDTYPE_PID);
			caller_mm = caller_task->mm;

			// 2. Check supplied pa_base_addr
			if (setup.pa_base_addr == NULL || (((uint64_t)setup.pa_base_addr) & 0xfff) != 0) {
				printk(KERN_WARNING "Setup: got invalid pa_base_addr. Must not be NULL and must be page-aligned.\n");
				return -1;
			}
			
			// 3. set up address translation table
			// for each page in the probe array (defined by setup.
			// pa_base_addr and setup.pa_size), we want to store the
			// physical address for later use.
			no_translation_table_entries = setup.pa_size / PAGE_SIZE;
			if (no_translation_table_entries > MAX_TRANSLATION_TABLE_ENTRIES) {
				printk(KERN_WARNING "Insufficient translation table size. Increase MAX_TRANSLATION_TABLE_ENTRIES to at least %zu.\n", no_translation_table_entries);
				return -1;
			}

			// 4. fill address translation table
			// query address translations and write them into translation_table.
			for (size_t i = 0; i < no_translation_table_entries; i++) {
				uint8_t* ptr = setup.pa_base_addr + i * PAGE_SIZE;
				uint64_t phys_addr = translate_virt_to_phys_addr(caller_mm, ptr);
				translation_table[i] = phys_addr;
			}

			// decrement usage counter in caller_task
			put_task_struct(caller_task);
			break;
		}
		// handle query request
		case IOCTL_QUERY: {
			enum cache_level level;
			size_t num_results_l1 = 0;
			size_t num_results_l2 = 0;

			// check if setup was completed before
			if (setup.pa_base_addr == NULL) {
				printk(KERN_WARNING "Got query without proper setup. Isse a setup request first.\n");
				return -1;
			}
			
			// query caches
			level = (enum cache_level)ioctl_param;
			if (level == CACHE_L1 || level == CACHE_ALL) {
				// NB: we call on_each_cpu, but the function called will only
				// query the L1 on one core and terminate on all others.
				on_each_cpu(query_l1, &num_results_l1, true);
			}
			if (level == CACHE_L2 || level == CACHE_ALL) {
				preempt_disable();
				num_results_l2 = run_query_l2(translation_table, no_translation_table_entries, l2_hits);
				preempt_enable();
			}
			
			// copy results to user space
			if (level == CACHE_L1 || level == CACHE_ALL) {
				// check size of supplied user space memory (+1 for initial count element)
				if (setup.resp_l1_size < (num_results_l1 + 1) * sizeof(size_t)) {
					printk(KERN_WARNING "memory area too small (L1)");
					return -1;
				}

				// copy results to userspace (target address supplied via request.mem_ptr)
				res = copy_to_user(setup.resp_l1, l1_hits, (num_results_l1 + 1) * sizeof(size_t));
				if (res != 0) {
					printk(KERN_WARNING "could not copy results to user space");
					return -1;
				}
			}
			if (level == CACHE_L2 || level == CACHE_ALL) {
				// check size of supplied user space memory (+1 for initial count element)
				if (setup.resp_l2_size < (num_results_l2 + 1) * sizeof(size_t)) {
					printk(KERN_WARNING "memory area too small (L2)");
					return -1;
				}

				// copy results to userspace (target address supplied via request.mem_ptr)
				res = copy_to_user(setup.resp_l2, l2_hits, (num_results_l2 + 1) * sizeof(size_t));
				if (res != 0) {
					printk(KERN_WARNING "could not copy results to user space");
					return -1;
				}
			}
			break;
		}
	}
	return 0;
}

// Module functions ***************************

struct file_operations Fops = {
	.unlocked_ioctl = device_ioctl,
	.open = device_open,
	.release = device_release // close
};

/**
 * @brief      Initialize the module and register the character device
 */
int init_module() {
	int ret_val;

	// We store AT MOST so may addresses in lx_stored_ptrs as there are
	// lines in lx cache. The first element of the list is an element
	// counter, so we need 1 additional entry.
	l1_hits = vzalloc((NUM_LINES_L1 + 1) * sizeof(size_t));
	l2_hits = vzalloc((NUM_LINES_L2 + 1) * sizeof(size_t));
	translation_table = vzalloc(MAX_TRANSLATION_TABLE_ENTRIES * sizeof(uint64_t));
	if (l1_hits == NULL || l2_hits == NULL || translation_table == NULL) {
		return -1;
	}

	// Register the character device (at least try)
	ret_val = register_chrdev(MAJOR_NUM, DEVICE_NAME, &Fops);

	// Negative values signify an error
	if (ret_val < 0) {
		printk(
			"Sorry, registering the character device failed with %d\n",
			ret_val
		);
		return ret_val;
	}

	printk(KERN_INFO "Registeration successful. The major device number is %d.\n", MAJOR_NUM);
	printk(
		KERN_INFO 
		"If you want to talk to the module, you'll have to create a device file.\n"
		"We suggest you use:\n"
	);
	printk(KERN_INFO "mknod %s c %d 0\n", DEVICE_FILE_NAME, MAJOR_NUM);

	return 0;
}

/**
 * @brief      Cleanup - unregister the appropriate file from /proc
 */
void cleanup_module() {
	// Unregister the device
	unregister_chrdev(MAJOR_NUM, DEVICE_NAME);

	// free memory for stored pointers
	vfree(l1_hits);
	vfree(l2_hits);
	vfree(translation_table);

	printk(KERN_INFO "Module unloaded\n");
}

MODULE_LICENSE("GPL");
