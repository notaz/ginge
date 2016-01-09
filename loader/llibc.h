void g_fprintf(int fd, const char *fmt, ...)
	__attribute__((format(printf, 2, 3)));

#define g_printf(fmt, ...) \
	g_fprintf(1, fmt, ##__VA_ARGS__)
