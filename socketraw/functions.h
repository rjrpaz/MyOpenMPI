#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

extern int r_open(const char *host, const char *pathname, int flags);
extern int r_close(int sd);
extern ssize_t r_read(int sd, void *buf, size_t posicion, size_t count);
extern ssize_t r_write(int sd, const void *buf, size_t count);
extern void error (char *);
extern void warning (char *);

extern int myprintf(const char *fmt, ...);
