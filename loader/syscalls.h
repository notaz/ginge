#include <stdlib.h>

struct timespec;

void sys_cacheflush(const void *start_addr, const void *end_addr);

long g_syscall(long number, ...);

// arg6 is func ptr, for convenience
long g_clone(unsigned long flags, void *child_stack, ...);

int  g_syscall_error(long kret);

// raw - no errno handling
long g_open_raw(const char *pathname, int flags, ...);
long g_read_raw(int fd, void *buf, size_t count);
long g_write_raw(int fd, const void *buf, size_t count);
long g_futex_raw(int *uaddr, int op, int val,
                 const struct timespec *timeout);
long g_nanosleep_raw(const struct timespec *req, struct timespec *rem);
long g_clock_gettime_raw(int clk_id, const struct timespec *tp);
long g_rt_sigprocmask_raw(int how, const void *set, void *oldset,
                         size_t sigsetsize);
