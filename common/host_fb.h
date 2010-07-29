int   host_video_init(int *stride, int no_dblbuf);
void *host_video_flip(void);

void host_video_blit4(const unsigned char *src, int w, int h, unsigned int *pal);
void host_video_blit8(const unsigned char *src, int w, int h, unsigned int *pal);
void host_video_blit16(const unsigned short *src, int w, int h);
