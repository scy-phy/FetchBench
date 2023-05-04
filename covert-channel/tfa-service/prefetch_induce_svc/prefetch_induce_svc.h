#include <assert.h>
#include <stdint.h>

#include <common/debug.h>
#include <common/runtime_svc.h>
#include <lib/el3_runtime/cpu_data.h>
#include <lib/pmf/pmf.h>
#include <lib/psci/psci.h>
#include <lib/runtime_instr.h>
//#include <services/gtsi_svc.h>
#include <services/pci_svc.h>
#include <services/rmmd_svc.h>
#include <services/sdei.h>
#include <services/spm_mm_svc.h>
#include <services/spmd_svc.h>
#include <services/std_svc.h>
#include <services/trng_svc.h>
#include <smccc_helpers.h>
#include <tools_share/uuid.h>

#include <lib/xlat_tables/xlat_tables_v2.h>

#include <bl31/interrupt_mgmt.h>
#include <lib/el3_runtime/context_mgmt.h>
#include "nop.h"
#include "secret.h"

#define CACHE_LINE_WIDTH (64)
#define REGION_SIZE (1024)

//#define FID_INUDCE_PATTERN		0xC3000001 // 0xC... = fast SMC call, 0x8... = yielding SMC call (bit 31)
#define FID_INDUCE_PATTERN		0xC3000002
//#define FID_INUDCE_PATTERN      0x43000001
#define FID_SET_UP_COUNTER		0x43000002
#define FID_INCREMENT_COUNTER	0x43000003

#define FID_CALL_COUNT	0xC300ff00
#define FID_UID			0xC300ff01
#define FID_REVISION	0xC300ff03

#define PATTERN_SIZE 2	// 2 bytes
#define SMS_ENTRIES 4
#define PAYLOAD_SIZE (SMS_ENTRIES * PATTERN_SIZE)
#define DATA_ARRAY_SIZE (2 * REGION_SIZE * SMS_ENTRIES)
#define MAX_RETRY 5

size_t seq = 0;
size_t retry = 0;

/* Service UUID (randomly generated) */
static uuid_t arm_svc_uid = {
	{0x96, 0x70, 0x51, 0x18},
	{0x33, 0x7f},
	{0xc7, 0x1b},
	0x4e, 0x49,
	{0x7b, 0x0f, 0xb2, 0x88, 0x2c, 0x0f}
};

static inline __attribute__((always_inline)) void maccess(void *p) {
	volatile uint32_t value;
	asm volatile("LDR %w0, [%1]\n\t"
		: "=r" (value)
		: "r" (p)
	);
}

static inline __attribute__((always_inline)) void flush(void* ptr) {
	asm volatile("DC CIVAC, %0" :: "r"(ptr));
	asm volatile("DSB ISH");
	asm volatile("ISB");
}
static inline __attribute__((always_inline)) void mfence() {
	asm volatile("DSB SY");
	asm volatile("ISB");
}

// non inline maccess for creating multiple entries in SMS prefetcher
static __attribute__ ((noinline)) __attribute__((optimize("O0"))) void maccess_1(void* p) {
	maccess(p);
}
static __attribute__ ((noinline)) __attribute__((optimize("O0"))) void maccess_2(void* p) {
	maccess(p);
}
static __attribute__ ((noinline)) __attribute__((optimize("O0"))) void maccess_3(void* p) {
	maccess(p);
}
static __attribute__ ((noinline)) __attribute__((optimize("O0"))) void maccess_4(void* p) {
	maccess(p);
}
static __attribute__ ((noinline)) __attribute__((optimize("O0"))) void maccess_5(void* p) {
	maccess(p);
}

static void (*maccess_array [5]) (void *p) = {  maccess_1,
						maccess_2,
						maccess_3,
						maccess_4,
						maccess_5};
