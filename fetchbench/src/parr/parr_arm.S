.global pchase
.global test_pc
.global test_pc_align


.extern iters            
.extern blind_key        
.extern head    
.extern increment   
.extern blind_measure
.extern time_measure

#ifdef COUNTER_THREAD
  .extern timestamp
  #define TIME(t) ldr t, [x13]
  #warning Using Counter Thread
#elif defined (APPLE_MSR)
  #warning Using Apple MSR
  #define TIME(t) MRS t, s3_2_c15_c0_00
#elif defined (ARM_MSR)
  #warning Using ARM MSR
  #define TIME(t) MRS t, PMCCNTR_EL0
#else
  #error no timer defined (use COUNTER_THREAD, APPLE_MSR, or ARM_MSR)
#endif



.text

test_pc_align:
    // -- initialization --
    
    // head = mem:head (clobbers x0)
    adrp x0, head
    add x0, x0, :lo12:head
    ldr x3, [x0]
    
    // blind_key = mem:blind_key (clobbers x0)
    adrp x0, blind_key
    add x0, x0, :lo12:blind_key
    ldr x10, [x0]
    
    // blinded measure ptr = mem:blind_measure (clobbers x0)
    adrp x0, blind_measure
    add x0, x0, :lo12:blind_measure
    ldr x4, [x0]

#ifdef COUNTER_THREAD
    // x13 <- addr:timestamp
    adrp x13, timestamp
    add x13, x13, :lo12:timestamp 
#endif    
    
    b align_loop_0_head

    // unrolled loop with 15 iterations across multiple pages and with different page offsets
.balign 16384
align_loop_0_head:
    b align_loop_1_head
align_loop_0:
    // load from head pointer
    ldr x0, [x3]
    // unblind
    eor x0, x0, x10
    // access unblinded pointer
    ldr x0, [x0]
    // go to next entry in parr
    add x3, x3, 8
    b align_loop_1

.balign 16384    
align_loop_1_head:
    b align_loop_2_head
    nop
align_loop_1:
    // 2
    ldr x0, [x3]
    eor x0, x0, x10
    ldr x0, [x0]
    add x3, x3, 8
    b align_loop_2

.balign 16384    
align_loop_2_head:
    b align_loop_3_head
    nop
    nop
align_loop_2:
    // 3
    ldr x0, [x3]
    eor x0, x0, x10
    ldr x0, [x0]
    add x3, x3, 8
    b align_loop_3

.balign 16384    
align_loop_3_head:
    b align_loop_4_head
    nop
    nop
    nop
align_loop_3:
    // 4
    ldr x0, [x3]
    eor x0, x0, x10
    ldr x0, [x0]
    add x3, x3, 8
    b align_loop_4

.balign 16384    
align_loop_4_head:
    b align_loop_5_head
    nop
    nop
    nop
    nop
align_loop_4:
    // 5
    ldr x0, [x3]
    eor x0, x0, x10
    ldr x0, [x0]
    add x3, x3, 8
    b align_loop_5

.balign 16384    
align_loop_5_head:
    b align_loop_6_head
    nop
    nop
    nop
    nop
    nop
align_loop_5:
    // 6
    ldr x0, [x3]
    eor x0, x0, x10
    ldr x0, [x0]
    add x3, x3, 8
    b align_loop_6

.balign 16384    
align_loop_6_head:
    b align_loop_7_head
    nop
    nop
    nop
    nop
    nop
    nop
align_loop_6:
    // 7
    ldr x0, [x3]
    eor x0, x0, x10
    ldr x0, [x0]
    add x3, x3, 8
    b align_loop_7

.balign 16384    
align_loop_7_head:
    b align_loop_8_head
    nop
    nop
    nop
    nop
    nop
    nop
    nop
align_loop_7:
    // 8
    ldr x0, [x3]
    eor x0, x0, x10
    ldr x0, [x0]
    add x3, x3, 8
    b align_loop_8

.balign 16384    
align_loop_8_head:
    b align_loop_9_head
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
    ldr x0, [x3]
    eor x0, x0, x10
    ldr x0, [x0]
    add x3, x3, 8
    b align_loop_9

.balign 16384    
align_loop_9_head:
    b align_loop_10_head
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
    ldr x0, [x3]
    eor x0, x0, x10
    ldr x0, [x0]
    add x3, x3, 8
    b align_loop_10

.balign 16384    
align_loop_10_head:
    b align_loop_11_head
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
    ldr x0, [x3]
    eor x0, x0, x10
    ldr x0, [x0]
    add x3, x3, 8
    b align_loop_11

.balign 16384    
align_loop_11_head:
    b align_loop_12_head
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
    ldr x0, [x3]
    eor x0, x0, x10
    ldr x0, [x0]
    add x3, x3, 8
    b align_loop_12

.balign 16384    
align_loop_12_head:
    b align_loop_13_head
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
    ldr x0, [x3]
    eor x0, x0, x10
    ldr x0, [x0]
    add x3, x3, 8
    b align_loop_13

.balign 16384    
align_loop_13_head:
    b align_loop_14_head
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
    ldr x0, [x3]
    eor x0, x0, x10
    ldr x0, [x0]
    add x3, x3, 8
    b align_loop_14

.balign 16384    
align_loop_14_head:
    b align_loop_0
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
    ldr x0, [x3]
    eor x0, x0, x10
    ldr x0, [x0]
    add x3, x3, 8
    
    // reuse measurement code
    b pchase_end


test_pc:
    // -- initialization --
    
    // head = mem:head (clobbers x0)
    adrp x0, head
    add x0, x0, :lo12:head
    ldr x3, [x0]
    
    // blind_key = mem:blind_key (clobbers x0)
    adrp x0, blind_key
    add x0, x0, :lo12:blind_key
    ldr x10, [x0]
    
    // blinded measure ptr = mem:blind_measure (clobbers x0)
    adrp x0, blind_measure
    add x0, x0, :lo12:blind_measure
    ldr x4, [x0]

#ifdef COUNTER_THREAD
    // x13 <- addr:timestamp
    adrp x13, timestamp
    add x13, x13, :lo12:timestamp 
#endif

    
    // unrolled loop with 15 iterations (access the first 15 entries, each access in a different instruction)
    // 1
    // load from head pointer
    ldr x0, [x3]
    // unblind
    eor x0, x0, x10
    // access unblinded pointer
    ldr x0, [x0]
    // go to next entry in parr
    add x3, x3, 8
    // 2
    ldr x0, [x3]
    eor x0, x0, x10
    ldr x0, [x0]
    add x3, x3, 8
    // 3
    ldr x0, [x3]
    eor x0, x0, x10
    ldr x0, [x0]
    add x3, x3, 8
    // 4
    ldr x0, [x3]
    eor x0, x0, x10
    ldr x0, [x0]
    add x3, x3, 8
    // 5
    ldr x0, [x3]
    eor x0, x0, x10
    ldr x0, [x0]
    add x3, x3, 8
    // 6
    ldr x0, [x3]
    eor x0, x0, x10
    ldr x0, [x0]
    add x3, x3, 8
    // 7
    ldr x0, [x3]
    eor x0, x0, x10
    ldr x0, [x0]
    add x3, x3, 8
    // 8
    ldr x0, [x3]
    eor x0, x0, x10
    ldr x0, [x0]
    add x3, x3, 8
    // 9
    ldr x0, [x3]
    eor x0, x0, x10
    ldr x0, [x0]
    add x3, x3, 8
    // 10
    ldr x0, [x3]
    eor x0, x0, x10
    ldr x0, [x0]
    add x3, x3, 8
    // 11
    ldr x0, [x3]
    eor x0, x0, x10
    ldr x0, [x0]
    add x3, x3, 8
    // 12
    ldr x0, [x3]
    eor x0, x0, x10
    ldr x0, [x0]
    add x3, x3, 8
    // 13
    ldr x0, [x3]
    eor x0, x0, x10
    ldr x0, [x0]
    add x3, x3, 8
    // 14
    ldr x0, [x3]
    eor x0, x0, x10
    ldr x0, [x0]
    add x3, x3, 8
    // 15
    ldr x0, [x3]
    eor x0, x0, x10
    ldr x0, [x0]
    add x3, x3, 8
    
    // measure the stuff (recycle from the other function)
    b pchase_end
    

pchase:
// --- registers ---
// x0: temporary register for loads (addresses and ignored values)
// x1: constant iterations
// x2: current iteration
// x3: constant head pointer
// x4: blinded pointer to measure (will be overwritten by measured time after meausrement)
// x6: temporary register for time measurement (start)
// x7: temporary register for time measurement (end / delta)
// x8: increment between iterations
// x9: temporary register for calculations (loop condition)
// x10: constant blind key
// x13: pointer to timestamp

    // -- initialization --
    
    // iters = mem:iters (clobbers x0)
    adrp x0, iters
    add x0, x0, :lo12:iters
    ldr x1, [x0]
    
    // increment = mem:increment (clobbers x0)
    adrp x0, increment
    add x0, x0, :lo12:increment
    ldr x8, [x0]
    
    // current iteration = 0
    ldr x2, #0
    
    // head = mem:head (clobbers x0)
    adrp x0, head
    add x0, x0, :lo12:head
    ldr x3, [x0]
    
    // blind_key = mem:blind_key (clobbers x0)
    adrp x0, blind_key
    add x0, x0, :lo12:blind_key
    ldr x10, [x0]
    
    // blinded measure ptr = mem:blind_measure (clobbers x0)
    adrp x0, blind_measure
    add x0, x0, :lo12:blind_measure
    ldr x4, [x0]

#ifdef COUNTER_THREAD
    // x13 <- addr:timestamp
    adrp x13, timestamp
    add x13, x13, :lo12:timestamp 
#endif

    // -- iterate over array --
pchase_loop:

    // if(iterations - current iteration == 0) goto pchase_end (clobbers x9)
    sub x9, x1, x2
    cbz x9, pchase_end
   
    // cur iteration %= iterations (clobbers x9)
    // this is done so we don't speculatively access further pointers (and we don't have to rely on fences)
    udiv x9, x2, x1
    msub x2, x9, x1, x2
        
    // *(uint64_t*)(*(uint64_t*)((uint8_t*)head + current iteration * increment) ^ blind_key)  (clobbers x0)
    madd x0, x2, x8, x3
    ldr x0, [x0]
 
    eor x0, x0, x10
    ldr x0, [x0]
    
    // current iteration ++
    add x2, x2, #1
    
    // goto pchase_loop
    b pchase_loop
    
pchase_end:
    // -- measurements --
    // blind key for measurement is different
    ldr x10, #0x40
    add x10, x10, #0x2
    lsl x10, x10, #8
    add x10, x10, #0x42
    lsl x10, x10, #8
    add x10, x10, #0x42
    lsl x10, x10, #8
    add x10, x10, #0x42
    lsl x10, x10, #8
    add x10, x10, #0x42
    lsl x10, x10, #8
    add x10, x10, #0x42
    lsl x10, x10, #8
    add x10, x10, #0x42
    lsl x10, x10, #8
    add x10, x10, #0x42

#ifdef COUNTER_THREAD
    // make sure time value is cached
    TIME(x6)
#endif
    
    // DUMMY
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
   
    // blinded measure ptr = time(*blinded measure ptr) (clobbers x6, x7, x0)
    DSB ISH
    ISB
    TIME(x6)
    DSB ISH
    ISB
    eor x4, x4, x10
    ldr x0, [x4]
    DSB ISH
    ISB
    TIME(x7)
    DSB ISH
    ISB
    sub x4, x7, x6
    
    // mem:time_measure = blinded measure ptr (clobbers x0)
    adrp x0, time_measure
    add x0, x0, :lo12:time_measure
    str x4, [x0]
    
    // return 
    ret
