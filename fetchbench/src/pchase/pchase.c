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


#define BLIND_MEASURE_KEY 0x4242424242424242ull
#define CACHE_LINE_SIZE 64
#define PAGE_SIZE (4096 * 4)
#define THRASH_ALLOC ((uint64_t)1024 * 1024 * 1024)
#define ENTRIES_COUNT (20 * PAGE_SIZE / sizeof(struct entry))

#if defined(__x86_64__)
    #define maccess(x) asm volatile("mov (%0), %%rax" :: "r" (x) : "rax")
#else /* probably arm */
    #define maccess(x) asm volatile("ldr x0, [%0]\n" :: "r" (x) : "x1")
#endif /* architecture */

// linked list entry
struct entry {
    struct entry* next;
    char selected[2*CACHE_LINE_SIZE - sizeof(struct entry*)];
};

// memory containing the linked list (all on one page)
struct entry* entry_array;

uint64_t m_measure;
// amount of entries
uint64_t entries;
// blind pointers so prefetcher cannot detect them
uint64_t xor_key;
// blinded pointer to head
uint64_t head_blinded;
// pointer to measure (blinded)
uint64_t pref_blinded;

static struct entry* select_entry(){
    struct entry* result;
    do {
        result = &entry_array[rand() % ENTRIES_COUNT];
    } while(result->selected[0]);
    result->selected[0] = 1;
    return result;
}

static void thrash_cache(void* memory, uint64_t size){
    for(uint64_t i = 0; i < size; i += CACHE_LINE_SIZE){
        #if defined(__x86_64__)
            asm volatile("clflush (%0)" :: "r" ((uint64_t)memory + i));
        #else /* probably Arm */
            asm volatile("DC CIVAC, %0" :: "r" ((uint64_t)memory + i));
        #endif /* __x86_64__ */
    }
}

// function to initialize linked list with random order
static void init(int is_ref){
	memset(entry_array, 0, sizeof(struct entry) * ENTRIES_COUNT); 
	
	struct entry* ref = select_entry();
	struct entry* head = select_entry();
	struct entry* last = head;
	
	for(uint64_t i = 0; i < entries; i++) {
	    struct entry* cur = select_entry();
	    last->next = cur;
	    last = cur;
	}
	
	xor_key = (uint64_t)BLIND_MEASURE_KEY;
	head_blinded = (uint64_t)(void*)head ^ xor_key;
    pref_blinded = (uint64_t)(void*)(is_ref ? ref : last) ^ xor_key;
}

// actual pointer chase function (implemented in assembly)
void pchase();

int uint64_t_compare(const void* _a, const void* _b){
    uint64_t a = *(const uint64_t*)_a;
    uint64_t b = *(const uint64_t*)_b;
    if(a < b)
        return -1;
    if(b < a)
        return 1;
    return 0;
}

void access_pages(uint64_t* arr, uint64_t size){
     for(uint64_t i = 0; i <= size; i += 4096){
         void* target = (void*)&arr[i / sizeof(uint64_t)];
         maccess(target);
     }
}

int main(int argc, char** argv){
    #ifdef COUNTER_THREAD
        pthread_t thread;
        pthread_create(&thread, NULL, counter_thread_thread, NULL);
        usleep(10000);
    #endif /* COUNTER_THREAD */

    srand(time(NULL));

    entry_array = malloc(sizeof(struct entry) * ENTRIES_COUNT);
    memset(entry_array, 'A', sizeof(struct entry) * ENTRIES_COUNT);


    uint64_t max = ENTRIES_COUNT / 10;
    
    uint64_t MEASUREMENTS = atoll(argv[1]);

    uint64_t* out_ref = malloc(sizeof(uint64_t) * MEASUREMENTS);
    uint64_t* out_mes = malloc(sizeof(uint64_t) * MEASUREMENTS);

    for(entries = 0; entries < max; entries ++){

        for(uint64_t i = 0; i < MEASUREMENTS * 2; i++){
            init(i % 2);
            thrash_cache(entry_array, sizeof(struct entry) * ENTRIES_COUNT);
            access_pages(entry_array, sizeof(struct entry) * ENTRIES_COUNT);
            pchase();
            (i % 2 ? out_ref : out_mes)[i >> 1] = m_measure;
        }


        qsort(out_mes, MEASUREMENTS, sizeof(uint64_t), uint64_t_compare);
        qsort(out_ref, MEASUREMENTS, sizeof(uint64_t), uint64_t_compare);

        for(uint64_t i = 0; i < MEASUREMENTS; i ++){
            printf("%zu: %zu %zu\n", entries, out_mes[i], out_ref[i]);
        }
    }
    
    return 0;
}
