#ifndef BOSSAN_H
#define BOSSAN_H

#include "ruby.h"
#include <assert.h>
#include <fcntl.h>   
#include <stddef.h> 
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#ifdef linux
#include <sys/prctl.h>
#elif defined(__APPLE__) || defined(__FreeBSD__)
#include <sys/uio.h>
#endif
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>

#define DEVELOP

#ifdef DEVELOP
#define DEBUG(...) \
    do { \
        printf("%-40s %-26s%4u: ", __FILE__, __func__, __LINE__); \
        printf(__VA_ARGS__); \
        printf("\n"); \
    } while(0)
#define RDEBUG(...) \
    do { \
        printf("\x1B[31m%-40s %-26s%4u: ", __FILE__, __func__, __LINE__); \
        printf(__VA_ARGS__); \
        printf("\x1B[0m\n"); \
    } while(0)
#define GDEBUG(...) \
    do { \
        printf("\x1B[32m%-40s %-26s%4u: ", __FILE__, __func__, __LINE__); \
        printf(__VA_ARGS__); \
        printf("\x1B[0m\n"); \
    } while(0)
#define BDEBUG(...) \
    do { \
        printf("\x1B[1;34m%-40s %-26s%4u: ", __FILE__, __func__, __LINE__); \
        printf(__VA_ARGS__); \
        printf("\x1B[0m\n"); \
    } while(0)
#define YDEBUG(...) \
    do { \
        printf("\x1B[1;33m%-40s %-26s%4u: ", __FILE__, __func__, __LINE__); \
        printf(__VA_ARGS__); \
        printf("\x1B[0m\n"); \
    } while(0)
#else
#define DEBUG(...) do{}while(0)
#define RDEBUG(...) do{}while(0)
#define GDEBUG(...) do{}while(0)
#define BDEBUG(...) do{}while(0)
#define YDEBUG(...) do{}while(0)
#endif

#if __GNUC__ >= 3
# define likely(x)    __builtin_expect(!!(x), 1)
# define unlikely(x)    __builtin_expect(!!(x), 0)
#else
# define likely(x) (x)
# define unlikely(x) (x)
#endif

#endif
