@ vim:filetype=armasm
#include <sys/syscall.h>


.global sys_cacheflush @ const void *start_addr, const void *end_addr
sys_cacheflush:
    mov     r2, #0
#ifdef __ARM_EABI__
    /* EABI version */
    str     r7, [sp, #-4]!
    mov     r7, #(__ARM_NR_cacheflush & 0xff)
#if (__ARM_NR_cacheflush & 0x00ff00)
    orr     r7, r7, #(__ARM_NR_cacheflush & 0x00ff00)
#endif
#if (__ARM_NR_cacheflush & 0xff0000)
    orr     r7, r7, #(__ARM_NR_cacheflush & 0xff0000)
#endif
    swi     0
    ldr     r7, [sp], #4
#else
    /* OABI */
    swi     __ARM_NR_cacheflush
#endif
    bx      lr


#ifdef __ARM_EABI__
#error hm?
#endif

.global g_syscall
g_syscall:
    mov   r12, sp
    stmfd sp!, {r4, r5, r6}
    ldmia r12, {r4, r5, r6}
    swi   __NR_syscall
    ldmfd sp!, {r4, r5, r6}

.global g_syscall_error
g_syscall_error:
    cmn   r0, #4096
    bxcc  lr
    stmfd sp!, {r4, lr}
    rsb   r4, r0, #0
    bl    __errno_location
    str   r4, [r0]
    mov   r0, #-1
    ldmfd sp!, {r4, pc}

.global g_clone
g_clone:
    ldr   r12,[sp, #4]      @ arg6 - convenience func ptr
    str   r4, [sp, #-4]!
    ldr   r4, [sp, #4]      @ arg5
    swi   __NR_clone
    tst   r0, r0
    bxeq  r12               @ child
    cmn   r0, #4096
    ldr   r4, [sp], #4
    bcs   g_syscall_error
    bx    lr

@ raw - no errno
.macro raw_syscall_easy name nr
.global \name
\name:
    swi   \nr
    bx    lr
.endm

.macro raw_syscall name nr
.global \name
\name:
    mov   r12, sp
    stmfd sp!, {r4, r5, r6, lr}
    ldmia r12, {r4, r5, r6}
    swi   \nr
    ldmfd sp!, {r4, r5, r6, pc}
.endm

raw_syscall_easy g_open_raw,           __NR_open
raw_syscall_easy g_read_raw,           __NR_read
raw_syscall_easy g_write_raw,          __NR_write
raw_syscall      g_mmap2_raw,          __NR_mmap2
raw_syscall_easy g_munmap_raw,         __NR_munmap
raw_syscall_easy g_ioctl_raw,          __NR_ioctl
raw_syscall_easy g_close_raw,          __NR_close
raw_syscall_easy g_chdir_raw,          __NR_chdir
raw_syscall_easy g_futex_raw,          __NR_futex
raw_syscall_easy g_nanosleep_raw,      __NR_nanosleep
raw_syscall_easy g_readlink_raw,       __NR_readlink
raw_syscall_easy g_execve_raw,         __NR_execve
raw_syscall_easy g_clock_gettime_raw,  __NR_clock_gettime
raw_syscall_easy g_rt_sigprocmask_raw, __NR_rt_sigprocmask
raw_syscall_easy g_exit_group_raw,     __NR_exit_group
