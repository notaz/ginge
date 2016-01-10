int   host_video_init(int *stride, int no_dblbuf);
void  host_video_finish(void);
void *host_video_flip(void);
void  host_video_change_bpp(int bpp);

// these are mutually exclusive
void  host_video_update_pal16(unsigned short *pal);
void  host_video_update_pal32(unsigned int *pal);

void host_video_blit4(const unsigned char *src, int w, int h, int stride);
void host_video_blit8(const unsigned char *src, int w, int h, int stride);
void host_video_blit16(const unsigned short *src, int w, int h, int stride);

void host_video_normalize_ts(int *x1024, int *y1024);
