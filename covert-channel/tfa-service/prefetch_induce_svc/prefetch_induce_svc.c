#include "prefetch_induce_svc.h"

// helper functions for covert channel

/* Helper function to encode two bytes in the region and induce that pattern 
 * in cache. The encoded bytes in region are of form [char2]X[char1]
 * where X is the trigger at position 8
 */
static __attribute__ ((noinline)) __attribute__((optimize("O0"))) void induce_pattern(uint8_t *data, uint8_t c1, uint8_t c2, int id)
{
	maccess_array[id](data + (DATA_ARRAY_SIZE/SMS_ENTRIES) * id + 8 * CACHE_LINE_WIDTH);
	for (int i=0; i < 7; i++) {
		if (c1 & (1 << i)) {
			maccess_array[id](data + (DATA_ARRAY_SIZE/SMS_ENTRIES) * id +  i * CACHE_LINE_WIDTH);
		}
		if (c2 & (1 << i)) {
			maccess_array[id](data + (DATA_ARRAY_SIZE/SMS_ENTRIES) * id + (9 + i) * CACHE_LINE_WIDTH);
		}
	}
	mfence();
	// one access in new region to move the entry to prediction stage
	maccess_array[id](data +  (DATA_ARRAY_SIZE/SMS_ENTRIES) * id + REGION_SIZE + 8 * CACHE_LINE_WIDTH);
	mfence();
}

/*
 * This helper function handles Secure EL1 preemption. The preemption could be
 * due Non Secure interrupts or EL3 interrupts. In both the cases we context
 * switch to the normal world and in case of EL3 interrupts, it will again be
 * routed to EL3 which will get handled at the exception vectors.
 */
uint64_t tspd_handle_sp_preemption(void *handle)
{
	cpu_context_t *ns_cpu_context;

	assert(handle == cm_get_context(SECURE));
	cm_el1_sysregs_context_save(SECURE);
	/* Get a reference to the non-secure context */
	ns_cpu_context = cm_get_context(NON_SECURE);
	assert(ns_cpu_context);

	/*
	 * To allow Secure EL1 interrupt handler to re-enter TSP while TSP
	 * is preempted, the secure system register context which will get
	 * overwritten must be additionally saved. This is currently done
	 * by the TSPD S-EL1 interrupt handler.
	 */

	/*
	 * Restore non-secure state.
	 */
	cm_el1_sysregs_context_restore(NON_SECURE);
	cm_set_next_eret_context(NON_SECURE);

	/*
	 * The TSP was preempted during execution of a Yielding SMC Call.
	 * Return back to the normal world with SMC_PREEMPTED as error
	 * code in x0.
	 */
	SMC_RET1(ns_cpu_context, SMC_PREEMPTED);
}

/* Set up service */
static int32_t prefetch_induce_svc_setup(void) {
	#ifdef NS_TIMER_SWITCH
	NOTICE("**** NS_TIMER_SWITCH is enabled\n");
	#else
	NOTICE("**** NS_TIMER_SWITCH is disabled\n");
	#endif

	return 0;
}


static volatile uint64_t* shared_ctr;

/*
 * Top-level Standard Service SMC handler.
 */
static uintptr_t prefetch_induce_svc_smc_handler(uint32_t smc_fid,
				 u_register_t x1, u_register_t x2, u_register_t x3,
				 u_register_t x4, void *cookie, void *handle, u_register_t flags) {

	NOTICE("ENTER prefetch_induce_svc_smc_handler %d\n", smc_fid);
	/* clear top parameter bits if 32-bit SMC function */
	if (((smc_fid >> FUNCID_CC_SHIFT) & FUNCID_CC_MASK) == SMC_32) {
		x1 &= UINT32_MAX;
		x2 &= UINT32_MAX;
		x3 &= UINT32_MAX;
		x4 &= UINT32_MAX;
	}

	if (retry == MAX_RETRY) {
		retry = 0;
		seq += PAYLOAD_SIZE;
	}
	if (seq >= strlen(secret)) {
		seq = 0;
	}


	switch (smc_fid) {

	case FID_INDUCE_PATTERN: {
		VERBOSE("Get data...\n");
		uint8_t data[DATA_ARRAY_SIZE] __attribute__((aligned(1024))) ;

		for (size_t i = 0; i < DATA_ARRAY_SIZE; i += CACHE_LINE_WIDTH) {
			flush(data + i);
		}
		mfence();

		// pattern for payload 1
		for (size_t i = 0; i < SMS_ENTRIES; i++) {
			induce_pattern(data, secret[seq + i * 2], secret[seq + i * 2 + 1], i);
		}
		retry++;

		SMC_RET1(handle, seq);
	}

	case FID_SET_UP_COUNTER: {
		// expect x1 to be the physical address of the counter.
		uint64_t shared_ctr_phy_addr = (uint64_t)x1;
		// extract the base address of the page that contains the counter
		uint64_t shared_ctr_phy_addr_page = shared_ctr_phy_addr & ~0xfffULL;
		
		// add an address mapping to the page table for the page that
		// contains the counter
		mmap_add_region(
			shared_ctr_phy_addr_page,
			shared_ctr_phy_addr_page,
			4096,
			MT_MEMORY | MT_RW | MT_NS
		);

		shared_ctr = (uint64_t*)shared_ctr_phy_addr;

		//SMC_RET1(handle, ret == 0 ? 0 : 1);
		SMC_RET1(handle, 0);
	}

	case FID_INCREMENT_COUNTER: {
		for (uint64_t i = 0; i < 10000; i++) {
			*shared_ctr += 1;
		}

		SMC_RET1(handle, 0);
	}

	case FID_CALL_COUNT:
		 /* Return the number of Service Calls. */		 
		SMC_RET1(handle, 4);

	case FID_UID:
		/* Return UID to the caller */
		SMC_UUID_RET(handle, arm_svc_uid);

	case FID_REVISION:
		/* Return the version of current implementation */
		SMC_RET2(handle, 0x00, 0x01);

	default:
		VERBOSE("Unimplemented Standard Service Call: 0x%x \n", smc_fid);
		SMC_RET1(handle, SMC_UNK);
	}
}

/* Register Standard Service Calls as runtime service */
DECLARE_RT_SVC(
		prefetch_induce_svc,

		OEN_OEM_START,
		OEN_OEM_END,
		SMC_TYPE_FAST,
		prefetch_induce_svc_setup,
		prefetch_induce_svc_smc_handler
);
