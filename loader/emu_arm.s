@ vim:filetype=armasm
.text

@ save registers and flags and call emu_handle_op
@ r0 and r14 must be saved by caller, r0 is arg for handle_op
@ on return everything is restored except lr, which is used to return

.globl emu_call_handle_op
emu_call_handle_op:
    str   sp, [sp, #(-0xf00 + 4*13)]
    sub   sp, sp, #0xf00
    add   sp, sp, #4
    stmia sp, {r1-r12}
    sub   sp, sp, #4
    mrs   r2, cpsr
    mov   r1, sp
    str   r2, [sp, #4*15]
    mov   r4, lr
    bl    emu_handle_op
    ldr   r1, [sp, #4*15]
    mov   lr, r4
    msr   cpsr_f, r1
    ldmia sp, {r0-r13}
    bx    lr

