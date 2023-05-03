#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/mman.h>

#include <time.h>


#ifdef COUNTER_THREAD
    #include <pthread.h>
    volatile uint64_t timestamp;
    static void* counter_thread_thread(void* arg){
        for(;;){
            timestamp ++;
        }
    }
#endif /* COUNTER_THREAD */

/* Used by assembly */
uint64_t iters;
uint64_t blind_key;
uint64_t head;
uint64_t increment;
uint64_t blind_measure;
uint64_t blind_ref;
uint64_t time_measure;

/*
implemented in assembly.
Psudocode:
void pchase(){
    for(size_t i = 0; i <= iters; i++){
        uint64_t ptr = *(uint64_t*)(head + i * increment) ^ blind_key;
        *(uint64_t*)ptr;
    }
    time_measure = measure_time(*(uint64_t*)(blind_measure ^ blind_key));
    time_ref     = measure_time(*(uint64_t*)(blind_ref     ^ blind_key));
}
*/
void pchase();

/*
implemmented in assembly.
Pseudocode:
void test_pc(){
    // UNROLL loop
    for(size_t i = 0; i <= iters; i++){
        uint64_t ptr = *(uint64_t*)(head + i) ^ blind_key;
        *(uint64_t*)ptr;
    }
    time_measure = measure_time(*(uint64_t*)(blind_measure ^ blind_key));
    time_ref     = measure_time(*(uint64_t*)(blind_ref     ^ blind_key));
}
*/
void test_pc();

/*
implemented in assembly
basically test_pc only that instructions are on different pages and are on different page offsets
*/
void test_pc_align();

#define BLIND_MEASURE_KEY 0x4242424242424242ull

static uint64_t rand_seed;

static uint64_t rand64() {
    return rand_seed = (164603309694725029ull * rand_seed) % 14738995463583502973ull;
}

static void setup_pointers(
    uint64_t* parr_base, 
    uint64_t parr_entries_count, 
    uint64_t parr_entry_size, 
    uint64_t tarr_base, 
    uint64_t tarr_entries_count,
    uint64_t tarr_entry_size,
    uint64_t blind_key,
    uint64_t ref_ptr_offset
){
    // +1 because we need a ref pointer that is not in the array
    if(tarr_entries_count + 1 < parr_entries_count){
        fprintf(stderr, "pointer array is too big for target array (%zu vs. %zu)\n", parr_entries_count, tarr_entries_count);
        exit(1);
    }
    
    // entries in target array that are already used
    uint8_t* used = calloc(tarr_entries_count, sizeof(uint8_t));
    
    uint64_t idx = rand64() % tarr_entries_count;
    used[idx] = 1;
    blind_measure = (tarr_base + idx * tarr_entry_size * sizeof(uint64_t) + ref_ptr_offset) ^ BLIND_MEASURE_KEY;
    
    for(uint64_t i = 0; i < parr_entries_count; i++){
        do {
            idx = rand64() % tarr_entries_count;
        } while(used[idx]);
        
        parr_base[i * parr_entry_size] = (tarr_base + idx * tarr_entry_size * sizeof(uint64_t)) ^ blind_key;
        used[idx] = 1;
    }    
    
    free(used);
}

#if defined(__x86_64__)
    #define maccess(x) asm volatile("mov (%0), %%rax" :: "r" (x) : "rax")
#else /* probably arm */
    #define maccess(x) asm volatile("ldr x0, [%0]\n" :: "r" (x) : "x1")
#endif /* architecture */


#ifdef FLUSHING
  #warning Using Flushing
  #if defined(__x86_64__)
  uint64_t thrash_cache(void* memory, uint64_t size) {
      for(uint64_t offset = 0; offset < size; offset += 64){
          asm volatile("clflush (%0)" :: "r" ((uint64_t)memory + offset));
      }
  }

  #else /* probably arm */
  
  uint64_t thrash_cache(void* memory, uint64_t size) {
      for(uint64_t offset = 0; offset < size; offset += 64){
          asm volatile("DC CIVAC, %0" :: "r" ((uint64_t)memory + offset));
      }
  }
  
#endif /* architecture */
#elif defined (EVICTION)
  #warning Using Eviction
  uint64_t cache_thrash_array[1024*1024*128];
  uint64_t thrash_cache(void* memory, uint64_t size){
      uint64_t result = 0;
      for(uint64_t i = 8; i < sizeof(cache_thrash_array) / 8 - 8; i += 8){
          maccess(&cache_thrash_array[i - 8]);
          maccess(&cache_thrash_array[i]);
          maccess(&cache_thrash_array[i + 8]);
        //cache_thrash_array[i] = 5;
      } 
      return result;
  }
#else
  #error No cache control defined, use FLUSHING, or EVICTION
#endif /* cache control */

void access_pages(uint64_t* arr, uint64_t size){
     for(uint64_t i = 0; i <= size; i += 4096){
         void* target = (void*)&arr[i / sizeof(uint64_t)];
         maccess(target);
     }
}

static void run_pc_experiment(
    uint64_t repeat,
    uint64_t* measure_out, // array to write timings for measured pointer to
    uint64_t* ref_out, // array to write timings for ref pointer to
    uint64_t _blind_key
) {
   uint64_t _parr_size = sizeof(uint64_t*) * 1 * 512 + 4096;
   _parr_size += 4096 * 4 - (_parr_size % (4096 * 4));
   uint64_t _tarr_size = sizeof(uint64_t ) * 16 * 60000 + 4096 + (4096 * 4);
   _tarr_size += 4096 * 4 - (_tarr_size % (4096 * 4));
   
   uint64_t* parr = mmap(NULL, _parr_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
   uint64_t* tarr = mmap(NULL, _tarr_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
 
   for(size_t i = 0; i < _tarr_size / 8; i++){
       tarr[i] = rand64();
   }
   
   blind_key = _blind_key;
   
   increment = 1 * 8;
   head = (uint64_t) &parr[0];
   
   for(size_t r = 0; r < 2 * repeat; r++){
       memset(parr, 0, _parr_size);
       setup_pointers(parr, 512, 1, (uint64_t)tarr + 4096 * 4, 600, 16, blind_key, 0);
       if(r % 2){
           blind_measure = (parr[16] + 0) ^ blind_key ^ BLIND_MEASURE_KEY; 
       }
       thrash_cache(parr, _parr_size);
       thrash_cache(tarr, _tarr_size);
       
       access_pages(tarr, _tarr_size);
       test_pc();
       
       if(r % 2){
           measure_out[r / 2] = time_measure;
       } else {
           ref_out[r / 2] = time_measure;
       }
   }
   
   munmap(parr, _parr_size);
   munmap(tarr, _tarr_size);
}

static void run_pc_align_experiment(
    uint64_t repeat,
    uint64_t* measure_out, // array to write timings for measured pointer to
    uint64_t* ref_out, // array to write timings for ref pointer to
    uint64_t _blind_key
) {
   uint64_t _parr_size = sizeof(uint64_t*) * 1 * 512 + 4096;
   _parr_size += 4096 * 4 - (_parr_size % (4096 * 4));
   uint64_t _tarr_size = sizeof(uint64_t ) * 16 * 60000 + 4096 + (4096 * 4);
   _tarr_size += 4096 * 4 - (_tarr_size % (4096 * 4));
   
   uint64_t* parr = mmap(NULL, _parr_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
   uint64_t* tarr = mmap(NULL, _tarr_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
 
   for(size_t i = 0; i < _tarr_size / 8; i++){
       tarr[i] = rand64();
   }
   
   blind_key = _blind_key;
   
   increment = 1 * 8;
   head = (uint64_t) &parr[0];
   
   for(size_t r = 0; r < 2 * repeat; r++){
       memset(parr, 0, _parr_size);
       setup_pointers(parr, 512, 1, (uint64_t)tarr + 4096 * 4, 600, 16, blind_key, 0);
       if(r % 2){
           blind_measure = (parr[16] + 0) ^ blind_key ^ BLIND_MEASURE_KEY;
       }
       thrash_cache(parr, _parr_size); 
       thrash_cache(tarr, _tarr_size);
       access_pages(tarr, _tarr_size);
       test_pc_align();
       
       if(r % 2){
           measure_out[r / 2] = time_measure;
       } else {
           ref_out[r / 2] = time_measure;
       }
   }
   
   munmap(parr, _parr_size);
   munmap(tarr, _tarr_size);
}

static void run_experiment(
   size_t parr_entries_count,  // amount of entries in pointer array [ in parr_entry_size ]
   size_t parr_entry_size, // amount of space in between pointers (independent of space between accesses) [ in pointers ]
   size_t tarr_entries_count, // amount of integers in accessed array [ in tarr_entry_size ]
   size_t tarr_entry_size, // size of a single entry in target array [in uint64_ts ]
   size_t iter_increment, // amount of space between accesses [ in pointers ]
   size_t _iters, // amount of loop iterations
   size_t repeat, // how often to repeat the experiment
   size_t measure_index, // index to measure 
   uint64_t* measure_out, // array to write timings for measured pointer to
   uint64_t* ref_out, // array to write timings for ref pointer to
   uint64_t _blind_key, // xor key to use to encrypt pointers
   uint64_t ptr_offset, // offset from start of (maybe) cached entry to measure
   uint64_t iter_start_idx // start index for iteration
) {
   
   uint64_t _parr_size = sizeof(uint64_t*) * parr_entry_size * parr_entries_count + 4096;
   _parr_size += 4096 * 4 - (_parr_size % (4096 * 4));
   uint64_t _tarr_size = sizeof(uint64_t ) * tarr_entry_size * tarr_entries_count + 4096 + (4096 * 4);
   _tarr_size += 4096 * 4 - (_tarr_size % (4096 * 4));
   
   uint64_t* parr = mmap(NULL, _parr_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
   uint64_t* tarr = mmap(NULL, _tarr_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
 
   for(size_t i = 0; i < _tarr_size / 8; i++){
       tarr[i] = rand64();
   }
   
   blind_key = _blind_key;
   iters = _iters;
   increment = iter_increment * 8;
   head = (uint64_t) &parr[iter_start_idx];
   
   for(size_t r = 0; r < 2 * repeat; r++){
       memset(parr, 0, sizeof(uint64_t*) * parr_entry_size * parr_entries_count);
       setup_pointers(parr, parr_entries_count, parr_entry_size, (uint64_t)tarr + 4096 * 4, tarr_entries_count, tarr_entry_size, blind_key, ptr_offset);
       if(r % 2){
           blind_measure = (parr[measure_index * parr_entry_size] + ptr_offset) ^ blind_key ^ BLIND_MEASURE_KEY;
       }
       thrash_cache(parr, _parr_size);
       thrash_cache(tarr, _tarr_size);
       access_pages(tarr, _tarr_size);
       
       
       pchase();
       
       if(r % 2){
           measure_out[r / 2] = time_measure;
       } else {
           ref_out[r / 2]     = time_measure;
       }
   }
   
   munmap(parr, _parr_size);
   munmap(tarr, _tarr_size);
}

int uint64_t_compare(const void* _a, const void* _b){
    uint64_t a = *(const uint64_t*)_a;
    uint64_t b = *(const uint64_t*)_b;
    if(a < b)
        return -1;
    if(b < a)
        return 1;
    return 0;
}

int main(int argc, char** argv){
    #ifdef COUNTER_THREAD
        pthread_t thread;
        pthread_create(&thread, NULL, counter_thread_thread, NULL);
        usleep(10000);
    #endif /* COUNTER_THREAD */
    
    // seed random
    rand_seed = time(NULL);
    
    printf("; seed: %zu\n", rand_seed);
    
    #ifdef EVICTION
    for(size_t i = 0; i < sizeof(cache_thrash_array) / sizeof(uint64_t); i++){
        cache_thrash_array[i] = rand64();
    }
    #endif /* EVICTION */
    
    // #define ITERS 200    
    uint64_t ITERS = (uint64_t)atol(argv[1]);
    
    
    uint64_t* measure_out = malloc(ITERS * sizeof(uint64_t));
    uint64_t* ref_out     = malloc(ITERS * sizeof(uint64_t));
    
    // test for existence of the prefetcher
    printf("#[Experiment][existence]: start\n");
    run_experiment(
        512,         // 512 entries in pointer array
        1,           // no space between pointers
        60000,         // 600 entries in target array
        16,          // 128 byte entries
        1,           // go to direct next pointer when iterating
        120,         // amount of training pointers
        ITERS,       // amount of iterations
        120,         // access pointer directly after (possibly) prefetched entry
        measure_out, // output for measurement timings
        ref_out,     // output for reference timings
        0,           // no blinding here
        0,           // measure directly at (possibly) prefetched entry
        0            // start training with index 0
    );
    qsort(measure_out, ITERS, sizeof(uint64_t), uint64_t_compare);
    qsort(ref_out, ITERS, sizeof(uint64_t), uint64_t_compare);
    for(size_t i = 0; i < ITERS; i++){
        printf("%zu %zu\n", measure_out[i], ref_out[i]);
    }
    printf("#[Experiment][existence]: end\n");
    
    // test for existence of the prefetcher (use blinding, should rule out false positives)
    printf("#[Experiment][existence-ref]: start\n");
    run_experiment(
        512,         // 512 entries in pointer array
        1,           // no space between pointers
        60000,         // 600 entries in target array
        16,          // 128 byte entries
        1,           // go to direct next pointer when iterating
        120,         // amount of training pointers
        ITERS,       // amount of iterations
        120,         // access pointer directly after (possibly) prefetched entry
        measure_out, // output for measurement timings
        ref_out,     // output for reference timings
        0x42434445,  // do blinding here
        0,           // measure directly at (possibly) prefetched entry
        0            // start training with index 0
    );
    qsort(measure_out, ITERS, sizeof(uint64_t), uint64_t_compare);
    qsort(ref_out, ITERS, sizeof(uint64_t), uint64_t_compare);
    for(size_t i = 0; i < ITERS; i++){
        printf("%zu %zu\n", measure_out[i], ref_out[i]);
    }
    printf("#[Experiment][existence-ref]: end\n");
    
    // test whether the prefetcher detects backwards iteration
    printf("#[Experiment][backwards]: start\n");
    run_experiment(
        512,         // 512 entries in pointer array
        1,           // no space between pointers
        60000,         // 600 entries in target array
        16,          // 128 byte entries
        (uint64_t)-1,// go to direct previous pointer when iterating
        120,         // amount of training pointers
        ITERS,       // amount of iterations
        0,           // access pointer directly after (possibly) prefetched entry
        measure_out, // output for measurement timings
        ref_out,     // output for reference timings
        0,           // no blinding here
        0,           // measure directly at (possibly) prefetched entry
        121          // start training with index 121
    );
    qsort(measure_out, ITERS, sizeof(uint64_t), uint64_t_compare);
    qsort(ref_out, ITERS, sizeof(uint64_t), uint64_t_compare);
    for(size_t i = 0; i < ITERS; i++){
        printf("%zu %zu\n", measure_out[i], ref_out[i]);
    }
    printf("#[Experiment][backwards]: end\n");
    
    // test whether the prefetcher detects backwards iteration
    printf("#[Experiment][backwards-ref]: start\n");
    run_experiment(
        512,         // 512 entries in pointer array
        1,           // no space between pointers
        60000,         // 600 entries in target array
        16,          // 128 byte entries
        (uint64_t)-1,// go to direct previous pointer when iterating
        120,         // amount of training pointers
        ITERS,       // amount of iterations
        0,           // access pointer directly after (possibly) prefetched entry
        measure_out, // output for measurement timings
        ref_out,     // output for reference timings
        0xcafebabe,  // use blinding here
        0,           // measure directly at (possibly) prefetched entry
        121          // start training with index 121
    );
    qsort(measure_out, ITERS, sizeof(uint64_t), uint64_t_compare);
    qsort(ref_out, ITERS, sizeof(uint64_t), uint64_t_compare);
    for(size_t i = 0; i < ITERS; i++){
        printf("%zu %zu\n", measure_out[i], ref_out[i]);
    }
    printf("#[Experiment][backwards-ref]: end\n");
    
    
    // test whether prefetching depends on pc
    printf("#[Experiment][pc-dependence]: start\n");
    run_pc_experiment(
        ITERS, measure_out, ref_out, 0
    );
    qsort(measure_out, ITERS, sizeof(uint64_t), uint64_t_compare);
    qsort(ref_out, ITERS, sizeof(uint64_t), uint64_t_compare);
    for(size_t i = 0; i < ITERS; i++){
        printf("%zu %zu\n", measure_out[i], ref_out[i]);
    }
    printf("#[Experiment][pc-dependence]: end\n");
    
    // reference test for whether prefetching depends on pc
    printf("#[Experiment][pc-dependence-ref]: start\n");
    run_pc_experiment(
        ITERS, measure_out, ref_out, 0x42434445
    );
    qsort(measure_out, ITERS, sizeof(uint64_t), uint64_t_compare);
    qsort(ref_out, ITERS, sizeof(uint64_t), uint64_t_compare);
    for(size_t i = 0; i < ITERS; i++){
        printf("%zu %zu\n", measure_out[i], ref_out[i]);
    }
    printf("#[Experiment][pc-dependence-ref]: end\n");
    
    // test whether prefetching depends on pc (on different pages at different offsets)
    printf("#[Experiment][pc-dependence-align]: start\n");
    run_pc_align_experiment(
        ITERS, measure_out, ref_out, 0
    );
    qsort(measure_out, ITERS, sizeof(uint64_t), uint64_t_compare);
    qsort(ref_out, ITERS, sizeof(uint64_t), uint64_t_compare);
    for(size_t i = 0; i < ITERS; i++){
        printf("%zu %zu\n", measure_out[i], ref_out[i]);
    }
    printf("#[Experiment][pc-dependence-align]: end\n");
    
    // reference test for whether prefetching depends on pc
    printf("#[Experiment][pc-dependence-align-ref]: start\n");
    run_pc_align_experiment(
        ITERS, measure_out, ref_out, 0x42434445
    );
    qsort(measure_out, ITERS, sizeof(uint64_t), uint64_t_compare);
    qsort(ref_out, ITERS, sizeof(uint64_t), uint64_t_compare);
    for(size_t i = 0; i < ITERS; i++){
        printf("%zu %zu\n", measure_out[i], ref_out[i]);
    }
    printf("#[Experiment][pc-dependence-align-ref]: end\n");
    
    // test different amounts of training pointers
    printf("#[Experiment][training-pointers]: start\n");
    for(uint64_t o = 1; o <= 16; o++){
        run_experiment(
            512,         // 512 entries in pointer array
            8,           // no space between pointers
            60000,         // 600 entries in target array
            16,          // 128 byte entries
            8,           // go to direct next pointer when iterating
            o,           // amount of training pointers
            ITERS,       // amount of iterations
            o,           // access pointer directly after (possibly) prefetched entry
            measure_out, // output for measurement timings
            ref_out,     // output for reference timings
            0,           // no blinding here
            0,           // measure directly at (possibly) prefetched entry
            0            // start training with index 0
        );
        qsort(measure_out, ITERS, sizeof(uint64_t), uint64_t_compare);
        qsort(ref_out, ITERS, sizeof(uint64_t), uint64_t_compare);
        for(size_t i = 0; i < ITERS; i++){
            printf("%zu %zu %zu\n", o, measure_out[i], ref_out[i]);
        }
    }
    printf("#[Experiment][training-pointers]: end\n");
    
    // test different measurement offsets from (possibly) prefetched pointer
    printf("#[Experiment][target-offset]: start\n");
    for(uint64_t o = 0; o <= 1024; o += 8){
        run_experiment(
            512,         // 512 entries in pointer array
            1,           // no space between pointers
            60000,         // 600 entries in target array
            16,          // 128 byte entries
            1,           // go to direct next pointer when iterating
            120,         // amount of training pointers
            ITERS,       // amount of iterations
            120,         // access pointer directly after (possibly) prefetched entry
            measure_out, // output for measurement timings
            ref_out,     // output for reference timings
            0,           // no blinding here
            o-512,       // measure at currently tested offset (offset is in bytes)
            0            // start training with index 0
        );
        qsort(measure_out, ITERS, sizeof(uint64_t), uint64_t_compare);
        qsort(ref_out, ITERS, sizeof(uint64_t), uint64_t_compare);
        for(size_t i = 0; i < ITERS; i++){
            printf("%zd %zu %zu\n", (int64_t)(o-512), measure_out[i], ref_out[i]);
        }
    }
    printf("#[Experiment][target-offset]: end\n");
    
    // test different offsets from last training pointer to measure
    printf("#[Experiment][pointer-array-offset]: start\n");
    for(uint64_t o = 0; o <= 256; o++){
        run_experiment(
            512,         // 512 entries in pointer array
            1,           // no space between pointers
            60000,         // 600 entries in target array
            16,          // 128 byte entries
            1,           // go to direct next pointer when iterating
            120,         // amount of training pointers
            ITERS,       // amount of iterations
            120 + o,     // measure pointer at current offset from training pointer (0 is actually 1 :-) )
            measure_out, // output for measurement timings
            ref_out,     // output for reference timings
            0,           // no blinding here
            0,           // measure directly at (possibly) prefetched entry
            0            // start training with index 0
        );
        qsort(measure_out, ITERS, sizeof(uint64_t), uint64_t_compare);
        qsort(ref_out, ITERS, sizeof(uint64_t), uint64_t_compare);
        for(size_t i = 0; i < ITERS; i++){
            printf("%zu %zu %zu\n", o, measure_out[i], ref_out[i]);
        }
    }
    printf("#[Experiment][pointer-array-offset]: end\n");
    
    // test different offsets between pointers
    printf("#[Experiment][pointer-array-space]: start\n");
    for(uint64_t o = 1; o <= 128; o++){
        run_experiment(
            512,         // 512 entries in pointer array
            o,           // current space between pointers (in multiples of 8 bytes)
            60000,         // 600 entries in target array
            16,          // 128 byte entries
            o,           // iterate with current offset (in multiples of 8 bytes)
            120,         // amount of training pointers
            ITERS,       // amount of iterations
            120,         // access pointer directly after (possibly) prefetched entry
            measure_out, // output for measurement timings
            ref_out,     // output for reference timings
            0,           // no blinding here
            0,           // measure directly at (possibly) prefetched entry
            0            // start training with index 0
        );
        qsort(measure_out, ITERS, sizeof(uint64_t), uint64_t_compare);
        qsort(ref_out, ITERS, sizeof(uint64_t), uint64_t_compare);
        for(size_t i = 0; i < ITERS; i++){
            printf("%zu %zu %zu\n", o, measure_out[i], ref_out[i]);
        }
    }
    printf("#[Experiment][pointer-array-space]: end\n");
    
    free(measure_out);
    free(ref_out);
    exit(0);
}
