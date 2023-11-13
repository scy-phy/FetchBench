#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/arm-smccc.h>
#include <linux/proc_fs.h>

#include <asm/io.h>

#include "smc_ioctl.h"

// The name for the device, as it will appear in /proc/devices
#define SMC_DEVICE_NAME SMC_DEVICE_FILE_NAME

// Is the device open right now? Used to prevent concurent access into the same device.
static int smc_device_open_ctr = 0;
// ioctl functions ***************************

/**
 * @brief      Called whenever a process attempts to open the device file.
 *             allow only 1 process to open the device at a time
 */
static int device_open(struct inode *inode, struct file *file) {
	if (smc_device_open_ctr != 0) {
		return -EBUSY;
	}
	smc_device_open_ctr++;
	return 0;
}

/**
 * @brief      Called when a process closes the device file.
 *
 */
static int device_release(struct inode *inode, struct file *file) {
	smc_device_open_ctr--;	
	return 0;
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
	switch (ioctl_num) {
		// trigger a load sequence in EL3 that induces a prefetch pattern
		case SMC_IOCTL_INDUCE_PATTERN: {
			// struct to store the response
			struct arm_smccc_res smc_res = { 0 };

			// SMC request
			arm_smccc_1_1_smc(
				// function id
				ARM_SMCCC_CALL_VAL(
					ARM_SMCCC_FAST_CALL,
					ARM_SMCCC_SMC_64,
					ARM_SMCCC_OWNER_OEM,
					0x0002	// function ID
				),
				// pointer to result structure
				&smc_res
			);

			printk(
			 	KERN_INFO "response:\n a0: %lx\n a1: %lx\n a2: %lx\n a3: %lx\n",
			 	smc_res.a0, smc_res.a1, smc_res.a2, smc_res.a3
			 );
			break;
		}
		
		// store the pointer to a counter, stored at the physical address
		// indicated by ioctl_param (i.e. owned by userspace program), in
		// EL3.
		case SMC_IOCTL_SET_UP_COUNTER: {
			// struct to store the response
			struct arm_smccc_res smc_res = { 0 };

			// SMC request
			arm_smccc_1_1_smc(
				// function id
				ARM_SMCCC_CALL_VAL(
					ARM_SMCCC_FAST_CALL,
					ARM_SMCCC_SMC_64,
					ARM_SMCCC_OWNER_OEM,
					0x0001	// function ID
				),
				// pointer to result structure
				&smc_res
			);

			printk(
			 	KERN_INFO "response:\n a0: %lx\n a1: %lx\n a2: %lx\n a3: %lx\n",
			 	smc_res.a0, smc_res.a1, smc_res.a2, smc_res.a3
			 );
			break;
#if 0
			// struct to store the response
			struct arm_smccc_res smc_res = { 0 };

			// SMC request
			arm_smccc_1_1_smc(
				// function id
				ARM_SMCCC_CALL_VAL(
					ARM_SMCCC_FAST_CALL,
					ARM_SMCCC_SMC_64,
					ARM_SMCCC_OWNER_OEM,
					0x0002	// function ID
				),
				(uint64_t)ioctl_param,
				// pointer to result structure
				&smc_res
			);

			break;
#endif
		}

		// increment the counter at the address stored previously (via
		// SMC_IOCTL_SET_UP_COUNTER)
		case SMC_IOCTL_INCREMENT_COUNTER: {
			// struct to store the response
			struct arm_smccc_res smc_res = { 0 };

			// SMC request
			arm_smccc_1_1_smc(
				// function id
				ARM_SMCCC_CALL_VAL(
					ARM_SMCCC_FAST_CALL,
					ARM_SMCCC_SMC_64,
					ARM_SMCCC_OWNER_OEM,
					0x0003	// function ID
				),
				// pointer to result structure
				&smc_res
			);

			// printk(
			// 	KERN_INFO "response:\n a0: %lx\n a1: %lx\n a2: %lx\n a3: %lx\n",
			// 	smc_res.a0, smc_res.a1, smc_res.a2, smc_res.a3
			// );
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
 * @brief      Initialize the module: create the character device
 */
int init_module(void) {
	int ret_val;

	// Register the character device (at least try)
	ret_val = register_chrdev(SMC_MAJOR_NUM, SMC_DEVICE_NAME, &Fops);

	// Negative values signify an error
	if (ret_val < 0) {
		printk(
			"Sorry, registering the character device failed with %d\n",
			ret_val
		);
		return ret_val;
	}

	printk(KERN_INFO "Registeration successful. The major device number is %d.\n", SMC_MAJOR_NUM);
	printk(
		KERN_INFO 
		"If you want to talk to the module, you'll have to create a device file.\n"
		"We suggest you use:\n"
	);
	printk(KERN_INFO "mknod %s c %d 0\n", SMC_DEVICE_FILE_NAME, SMC_MAJOR_NUM);

	return 0;
}
 
/**
 * @brief      Cleanup - unregister character device
 */
void cleanup_module() {
	// Unregister the device
	unregister_chrdev(SMC_MAJOR_NUM, SMC_DEVICE_NAME);

	printk(KERN_INFO "Module unloaded\n");
}

MODULE_LICENSE("GPL");  // avoid "taints kernel" message
