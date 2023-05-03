.global pchase
.extern xor_key
.extern entries
.extern ref_blinded
.extern head_blinded
.extern m_measure

#ifdef COUNTER_THREAD
  .extern timestamp
  #define TIME(t) ldr t, [x15]
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

.data
.text

pchase:
#ifdef COUNTER_THREAD
    // timestamp
    adrp x15, timestamp
    add x15, x15, :lo12:timestamp
#endif /* COUNTER_THREAD */
// load counter
adrp x8, entries
add x8, x8, :lo12:entries
ldr x8, [x8]
// load xor key
adrp x12, xor_key
add x12, x12, :lo12:xor_key
ldr x12, [x12]
// load head pointer
adrp x6, head_blinded
add x6, x6, :lo12:head_blinded
ldr x6, [x6]
// load blinded pointer
adrp x13, pref_blinded
add x13, x13, :lo12:pref_blinded
ldr x13, [x13]
// 'decrypt' head pointer
eor x6, x6, x12
pchase_start:
// check counter == 0
cbz x8, pchase_end
DSB ISH
ISB
// do pointer chase
ldr x6, [x6]
// decrement loop counter
sub x8, x8, 0x0000000000000001
// contine chasing pointers if necessary
b pchase_start
pchase_end:
eor x6, x6, x6
// measure time for (possibly) prefetched pointer
DSB ISH
ISB
TIME(x9)
DSB ISH
ISB
eor x13, x13, x12
ldr x3, [x13]
DSB ISH
ISB
TIME(x11)
DSB ISH
ISB
sub x11, x11, x9
// store measurement
adrp x9, m_measure
add x9, x9, :lo12:m_measure
str x11, [x9]
ret
