#include <stdio.h>
#include <inttypes.h>
#include <semaphore.h>
#include <time.h>

#include <mbedtls/aes.h>

#include "common_test.hh"
#include "cores.hh"

int main(int argc, char** argv) {
	// pin process to victim core
	move_process_to_cpu(0, CPU_CORE_VICTIM);
	timing_init();

	uint8_t plaintext[16] = {0};
	if (argc > 1) {
		uint8_t* outptr;
		const char* inptr;
		unsigned int tmp;
		for (
			inptr = argv[1], outptr = plaintext;
			*inptr != '\0' && *(inptr+1) != '\0';
			inptr += 2, outptr += 1
		) {
			if (sscanf(inptr, "%02x", &tmp) != 1) {
				puts("[VI] Plain text parse error");
				return 1;
			}
			*outptr = tmp;
		}
		printf("[V] Set plain text to: ");
		for (size_t j = 0; j < 16; j++) {
			printf("%.2x ", plaintext[j]);
		}
		puts("");
	} else {
		puts("[VI] Error: No plaintext provided");
		return 1;
	}

	// AES setup
	uint8_t key[] = { 
		#include "key.hh"
	};
	uint8_t output[16];

	// library call
	mbedtls_aes_context ctx;
	mbedtls_aes_init(&ctx);
	mbedtls_aes_setkey_enc(&ctx, key, 128);
	mbedtls_internal_aes_encrypt(&ctx, plaintext, output);
	
	// printf("[V] victim done on %d\n", get_current_cpu_core());
	
	return 0;
}
