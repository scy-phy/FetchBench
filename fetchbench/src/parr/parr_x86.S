.global pchase
.global test_pc
.global test_pc_align


.extern iters            
.extern blind_key        
.extern head    
.extern increment   
.extern blind_measure   
.extern time_measur

#ifdef COUNTER_THREAD
  #warning Using Counter Thread
  #define TIME(t) mov t, [rsi]
  .extern timestamp
#elif defined (RDTSC)
  #warning using rdtsc
  #define TIME(t) rdtsc; shl rdx, 32; or rdx, rax; mov t, rdx
#else
  #error no timer defined, use COUNTER_THREAD or RDTSC
#endif /* timer */

.text
.intel_syntax noprefix

test_pc_align:
    // -- initialization --
    
    // head = mem:head 
    mov r11, [head]
    
    // blind_key = mem:blind_key
    mov r14, [blind_key]
    
    // blinded measure ptr = mem:blind_measure
    mov r12, [blind_measure]

#ifdef COUNTER_THREAD    
    // x13 <- addr:timestamp
    lea rsi, [timestamp]
#endif

    jmp align_loop_0_head

    // unrolled loop with 15 iterations across multiple pages and with different page offsets
.balign 16384
align_loop_0_head:
    jmp align_loop_1_head
align_loop_0:
    mov rax, [r11]
    xor rax, r14
    mov rax, [rax]
    add r11, 8
    jmp align_loop_1

.balign 16384    
align_loop_1_head:
    jmp align_loop_2_head
    nop
align_loop_1:
    // 2
    mov rax, [r11]
    xor rax, r14
    mov rax, [rax]
    add r11, 8
    jmp align_loop_2

.balign 16384    
align_loop_2_head:
    jmp align_loop_3_head
    nop
    nop
align_loop_2:
    // 3
    mov rax, [r11]
    xor rax, r14
    mov rax, [rax]
    add r11, 8
    jmp align_loop_3

.balign 16384    
align_loop_3_head:
    jmp align_loop_4_head
    nop
    nop
    nop
align_loop_3:
    // 4
    mov rax, [r11]
    xor rax, r14
    mov rax, [rax]
    add r11, 8
    jmp align_loop_4

.balign 16384    
align_loop_4_head:
    jmp align_loop_5_head
    nop
    nop
    nop
    nop
align_loop_4:
    // 5
    mov rax, [r11]
    xor rax, r14
    mov rax, [rax]
    add r11, 8
    jmp align_loop_5

.balign 16384    
align_loop_5_head:
    jmp align_loop_6_head
    nop
    nop
    nop
    nop
    nop
align_loop_5:
    // 6
    mov rax, [r11]
    xor rax, r14
    mov rax, [rax]
    add r11, 8
    jmp align_loop_6

.balign 16384    
align_loop_6_head:
    jmp align_loop_7_head
    nop
    nop
    nop
    nop
    nop
    nop
align_loop_6:
    // 7
    mov rax, [r11]
    xor rax, r14
    mov rax, [rax]
    add r11, 8
    jmp align_loop_7

.balign 16384    
align_loop_7_head:
    jmp align_loop_8_head
    nop
    nop
    nop
    nop
    nop
    nop
    nop
align_loop_7:
    // 8
    mov rax, [r11]
    xor rax, r14
    mov rax, [rax]
    add r11, 8
    jmp align_loop_8

.balign 16384    
align_loop_8_head:
    jmp align_loop_9_head
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
align_loop_8:
    // 9
    mov rax, [r11]
    xor rax, r14
    mov rax, [rax]
    add r11, 8
    jmp align_loop_9

.balign 16384    
align_loop_9_head:
    jmp align_loop_10_head
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
align_loop_9:
    // 10
    mov rax, [r11]
    xor rax, r14
    mov rax, [rax]
    add r11, 8
    jmp align_loop_10

.balign 16384    
align_loop_10_head:
    jmp align_loop_11_head
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
align_loop_10:
    // 11
    mov rax, [r11]
    xor rax, r14
    mov rax, [rax]
    add r11, 8
    jmp align_loop_11

.balign 16384    
align_loop_11_head:
    jmp align_loop_12_head
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
align_loop_11:
    // 12
    mov rax, [r11]
    xor rax, r14
    mov rax, [rax]
    add r11, 8
    jmp align_loop_12

.balign 16384    
align_loop_12_head:
    jmp align_loop_13_head
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
align_loop_12:
    // 13
    mov rax, [r11]
    xor rax, r14
    mov rax, [rax]
    add r11, 8
    jmp align_loop_13

.balign 16384    
align_loop_13_head:
    jmp align_loop_14_head
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
align_loop_13:
    // 14    
    mov rax, [r11]
    xor rax, r14
    mov rax, [rax]
    add r11, 8
    jmp align_loop_14

.balign 16384    
align_loop_14_head:
    jmp align_loop_0
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
align_loop_14:
    // 15
    mov rax, [r11]
    xor rax, r14
    mov rax, [rax]
    add r11, 8
    
    // reuse measurement code
    jmp pchase_end

test_pc:
    // -- initialization --
    
    // head = mem:head 
    mov r11, [head]
    
    // blind_key = mem:blind_key
    mov r14, [blind_key]
    
    // blinded measure ptr = mem:blind_measure
    mov r12, [blind_measure]

#ifdef COUNTER_THREAD    
    // x13 <- addr:timestamp
    lea rsi, [timestamp]
#endif

    // unrolled loop with 15 iterations (access the first 15 entries, each access in a different instruction)
    // 1
    // load from head pointer
    mov rax, [r11]
    // unblind
    xor rax, r14
    // access unblinded pointer
    mov rax, [rax]
    // go to next entry in parr
    add r11, 8
    // 2
    mov rax, [r11]
    xor rax, r14
    mov rax, [rax]
    add r11, 8
    // 3
    mov rax, [r11]
    xor rax, r14
    mov rax, [rax]
    add r11, 8    
    // 4
    mov rax, [r11]
    xor rax, r14
    mov rax, [rax]
    add r11, 8
    // 5
    mov rax, [r11]
    xor rax, r14
    mov rax, [rax]
    add r11, 8
    // 6
    mov rax, [r11]
    xor rax, r14
    mov rax, [rax]
    add r11, 8
    // 7
    mov rax, [r11]
    xor rax, r14
    mov rax, [rax]
    add r11, 8
    // 8
    mov rax, [r11]
    xor rax, r14
    mov rax, [rax]
    add r11, 8
    // 9
    mov rax, [r11]
    xor rax, r14
    mov rax, [rax]
    add r11, 8
    // 10
    mov rax, [r11]
    xor rax, r14
    mov rax, [rax]
    add r11, 8
    // 11
    mov rax, [r11]
    xor rax, r14
    mov rax, [rax]
    add r11, 8
    // 12
    mov rax, [r11]
    xor rax, r14
    mov rax, [rax]
    add r11, 8
    // 13
    mov rax, [r11]
    xor rax, r14
    mov rax, [rax]
    add r11, 8
    // 14
    mov rax, [r11]
    xor rax, r14
    mov rax, [rax]
    add r11, 8
    // 15
    mov rax, [r11]
    xor rax, r14
    mov rax, [rax]
    add r11, 8
    
    // measure the stuff (recycle from the other function)
    jmp pchase_end


pchase:
// --- registers ---
// rax: temporary register for loads (addresses and ignored values)
// r9: constant iterations
// rcx: current iteration
// r11: constant head pointer
// r12: blinded pointer to measure (will be overwritten by measured time after meausrement)
// rdi: temporary register for time measurement (start)
// r15: temporary register for time measurement (end / delta)
// r8: increment between iterations
// r10: temporary register for calculations (loop condition)
// r14: constant blind key
// rsi: pointer to timestamp

    // -- initialization --
    
    // iters = mem:iters
    mov r9, [iters]
    
    // increment = mem:increment
    mov r8, [increment]
    
    // current iteration = 0
    mov rcx, 0
    
    // head = mem:head 
    mov r11, [head]
    
    // blind_key = mem:blind_key
    mov r14, [blind_key]
    
    // blinded measure ptr = mem:blind_measure
    mov r12, [blind_measure]

#ifdef COUNTER_THREAD    
    // x13 <- addr:timestamp
    lea rsi, [timestamp]
#endif
    
    // -- iterate over array --
pchase_loop:
    
    // if(iterations  == current iteration) goto pchase_end
    cmp rcx, r9
    je pchase_end
    
    // cur iteration %= iterations (clobbers rax)
    // this is done so we don't speculatively access further pointers (and we don't have to rely on fences)    
    mov rax, rcx
    mov rdx, 0
    div r9
    mov rcx, rdx
    
    // just put in all fences there are
    mfence
    lfence
    
    // *(uint64_t*)(*(uint64_t*)((uint8_t*)head + current iteration * increment) ^ blind_key)  (clobbers x0)
    mov rax, rcx
    mul r8
    add rax, r11
    mov rax, [rax]
    xor rax, r14
    mov rax, [rax] 
    
    // current iteration ++
    inc rcx
    
    // goto pchase_loop
    jmp pchase_loop

pchase_end:    
    // -- measurements --
    
    // blind key for measurement is different
    mov r14, 0x4242424242424242
    
    mfence
    
#ifdef COUNTER_THREAD
    // make sure value is cached
    mov rax, [rsi]
#endif
    
    // blinded measure ptr = time(*blinded measure ptr) (clobbers rax)
    lfence
    TIME(rdi)
    lfence
    xor r12, r14
    mov rax, [r12]
    lfence
    TIME(r15)
    lfence
    mov r12, r15
    sub r12, rdi
    
    // mem:time_measure = blinded measure ptr
    mov [time_measure], r12
    
    ret
pchase_measure_end:
    nop
