.text

/* void do_entry(Elf32_Addr entry, void *stack_frame, int stack_frame_elems, void *exitf); */

.globl do_entry
do_entry:
    sub   sp, sp, r2, lsl #2
    mov   r4, sp
    mov   r5, r0
0:
    ldr   r0, [r1], #4
    subs  r2, r2, #1
    str   r0, [r4], #4
    bgt   0b

/*
 r0 - atexit func
 sp - stack frame of:
   argc
   argv[0]
   ...
   NULL
   envp[0]
   ...
   NULL
*/
    mov   r0, r3
    bx    r5
