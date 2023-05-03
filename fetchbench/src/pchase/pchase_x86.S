.global pchase
.extern xor_key
.extern entries
.extern ref_blinded
.extern head_blinded
.extern m_measure

#ifdef COUNTER_THREAD
  #warn Using Counter Thread
  #define TIME(t) mov t, [rsi]
  .extern timestamp
#elif defined (RDTSC)
  #warn using rdtsc
  #define TIME(t) rdtsc; shl rdx, 32; or rdx, rax; mov t, rdx
#else
  #error no timer defined, use COUNTER_THREAD or RDTSC
#endif /* timer */

.data
.text
.intel_syntax noprefix
pchase:
#ifdef COUNTER_THREAD
    // load timestamp
    lea rsi, [rip+timestamp]
#endif
// load counter
lea r10, [rip+entries]
mov r10, [r10]
// load xor key
lea r14, [rip+xor_key]
mov r14, [r14]
// load head pointer
lea r8, [rip+head_blinded]
mov r8, [r8]
// load blinded pointer
lea r15, [rip+pref_blinded]
mov r15, [r15]
// 'decrypt' head pointer
xor r8, r14
pchase_start:
// check counter == 0
cmp r10, 0
je pchase_end
lfence
// do pointer chase
mov r8, [r8]
// decrement loop counter
sub r10, 0x0000000000000001
// contine chasing pointers if necessary
jmp pchase_start
pchase_end:
xor r8, r8
// measure time for (possibly) prefetched pointer
lfence
TIME(r11)
lfence
xor r15, r14
mov rax, [r15]
lfence
TIME(r13)
lfence
sub r13, r11
// store measurement
lea r11, [rip+m_measure]
mov [r11], r13
ret