@ vim:filetype=armasm

@ input: r2-r5
@ output: r7,r8
@ trash: r6
.macro rb_line_low
    mov     r6, r2, lsl #16
    mov     r7, r3, lsl #16
    orr     r7, r7, r6, lsr #16
    mov     r6, r4, lsl #16
    mov     r8, r5, lsl #16
    orr     r8, r8, r6, lsr #16
.endm

.macro rb_line_hi
    mov     r6, r2, lsr #16
    mov     r7, r3, lsr #16
    orr     r7, r6, r7, lsl #16
    mov     r6, r4, lsr #16
    mov     r8, r5, lsr #16
    orr     r8, r6, r8, lsl #16
.endm

.global rotated_blit16 @ void *dst, void *linesx4
rotated_blit16:
    stmfd   sp!,{r4-r8,lr}

    sub     r0, r0, #240*2 @ adjust
    mov     lr, #240/4

rotated_blit_loop16_o:
    orr     lr, lr, #((320/4)-1) << 16
    add     r0, r0, #(240*320)*2

rotated_blit_loop16:
    ldr     r2, [r1, #320*0*2]
    ldr     r3, [r1, #320*1*2]
    ldr     r4, [r1, #320*2*2]
    ldr     r5, [r1, #320*3*2]
    rb_line_low
    stmia   r0, {r7,r8}
    sub     r0, r0, #240*2
    rb_line_hi
    stmia   r0, {r7,r8}
    sub     r0, r0, #240*2

    ldr     r2, [r1, #320*0*2+4]
    ldr     r3, [r1, #320*1*2+4]
    ldr     r4, [r1, #320*2*2+4]
    ldr     r5, [r1, #320*3*2+4]
    rb_line_low
    stmia   r0, {r7,r8}
    sub     r0, r0, #240*2
    rb_line_hi
    stmia   r0, {r7,r8}
    sub     r0, r0, #240*2

    subs    lr, lr, #1<<16
    add     r1, r1, #8
    bpl     rotated_blit_loop16

    add     lr, lr, #1<<16
    subs    lr, lr, #1

    add     r0, r0, #4*2
    add     r1, r1, #(320*3)*2
    bgt     rotated_blit_loop16_o

    ldmfd   sp!,{r4-r8,pc}


.global rotated_blit8 @ void *dst, void *linesx4
rotated_blit8:
    stmfd   sp!,{r4-r8,lr}

    mov     r8, #320
    sub     r0, r0, #240	@ adjust
    mov     lr, #240/4

rotated_blit8_loop_o:
    orr     lr, lr, #((320/4)-1) << 16
    add     r0, r0, #(240*320)

rotated_blit8_loop:
    mov     r6, r1
    ldr     r2, [r6], r8
    ldr     r3, [r6], r8
    ldr     r4, [r6], r8
    ldr     r5, [r6], r8

    mov     r6, r2, lsl #24
    mov     r6, r6, lsr #8
    orr     r6, r6, r3, lsl #24
    mov     r6, r6, lsr #8
    orr     r6, r6, r4, lsl #24
    mov     r6, r6, lsr #8
    orr     r6, r6, r5, lsl #24
    str     r6, [r0], #-240

    and     r6, r3, #0xff00
    and     r7, r2, #0xff00
    orr     r6, r6, r7, lsr #8
    and     r7, r4, #0xff00
    orr     r6, r6, r7, lsl #8
    and     r7, r5, #0xff00
    orr     r6, r6, r7, lsl #16
    str     r6, [r0], #-240

    and     r6, r4, #0xff0000
    and     r7, r2, #0xff0000
    orr     r6, r6, r7, lsr #16
    and     r7, r3, #0xff0000
    orr     r6, r6, r7, lsr #8
    and     r7, r5, #0xff0000
    orr     r6, r6, r7, lsl #8
    str     r6, [r0], #-240

    mov     r6, r5, lsr #24
    mov     r6, r6, lsl #8
    orr     r6, r6, r4, lsr #24
    mov     r6, r6, lsl #8
    orr     r6, r6, r3, lsr #24
    mov     r6, r6, lsl #8
    orr     r6, r6, r2, lsr #24
    str     r6, [r0], #-240

    subs    lr, lr, #1<<16
    add     r1, r1, #4
    bpl     rotated_blit8_loop

    add     lr, lr, #1<<16
    subs    lr, lr, #1

    add     r0, r0, #4
    add     r1, r1, #320*3
    bgt     rotated_blit8_loop_o

    ldmfd   sp!,{r4-r8,pc}

