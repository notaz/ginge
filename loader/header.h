
void do_entry(unsigned long entry, void *stack_frame, int stack_frame_cnt, void *exitf);

void do_patches(void *ptr, unsigned int size);

void  emu_init(void *map_bottom);
void *emu_mmap_dev(unsigned int length, int prot, int flags, unsigned int offset);

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

