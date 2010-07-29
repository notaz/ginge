#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <termios.h>


int   real_open(const char *pathname, int flags, ...);
FILE *real_fopen(const char *path, const char *mode);
void *real_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
int   real_read(int fd, void *buf, size_t count);
int   real_ioctl(int fd, int request, void *argp);
int   real_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);
typedef struct sigaction sigaction_t;
int   real_tcgetattr(int fd, struct termios *termios_p);
int   real_tcsetattr(int fd, int optional_actions, const struct termios *termios_p);
int   real_system(const char *command);

#define open real_open
#define fopen real_fopen
#define mmap real_mmap
#define read real_read
#define ioctl real_ioctl
#define sigaction real_sigaction
#define tcgetattr real_tcgetattr
#define tcsetattr real_tcsetattr
#define system real_system

