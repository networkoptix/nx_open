/*
 * ldpreloadhook - a quick open/close/ioctl/read/write/free/strcmp/strncmp symbol hooker
 *
 * Based on ldpreloadhook (hook.c) by Pau Oliva Fora <pof@eslack.org> 2012-2013.
 *
 * Based on vsound 0.6 source code:
 *   Copyright (C) 2004 Nathan Chantrell <nsc@zorg.org>
 *   Copyright (C) 2003 Richard Taylor <r.taylor@bcs.org.uk>
 *   Copyright (C) 2000,2001 Erik de Castro Lopo <erikd@zip.com.au>
 *   Copyright (C) 1999 James Henstridge <james@daa.com.au>
 * Based on esddsp utility that is part of esound:
 *   Copyright (C) 1998, 1999 Manish Singh <yosh@gimp.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * 1) Compile:
 *   gcc -fPIC -c -o hook.o hook.c
 *   gcc -shared -o hook.so hook.o -ldl
 * 2) Usage:
 *   LD_PRELOAD="./hook.so" command
 *   LD_PRELOAD="./hook.so" SPYFILE="/file/to/spy" command
 *   LD_PRELOAD="./hook.so" SPYFILE="/file/to/spy" DELIMITER="***" command
 * to spy the content of buffers free'd by free(), set the environment
 * variable SPYFREE, for example:
 *   LD_PRELOAD="./hook.so" SPYFREE=1 command
 * to spy the strings compared using strcmp(), set the environment
 * variable SPYSTR, for example:
 *   LD_PRELOAD="./hook.so" SPYSTR=1 command
 */

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <signal.h>

#include "hook_common.h"
#include "dev_fb.h"
#include "handle_fb_disp.h"

CONF_DEFINITION

#if !defined(RTLD_NEXT)
    #define RTLD_NEXT ((void *) -1l)
#endif

#define REAL_LIBC RTLD_NEXT

typedef void (*sighandler_t)(int);

#if defined(HOOK_WRITE)
    static int data_w_fd = -1;
#endif // HOOK_WRITE

#if !defined(CONF_IOCTL_SPECIAL)
    static int hook_fd = -1;
    const char* spy_file = "UNDEFINED";
#endif // !CONF_IOCTL_SPECIAL

#if defined(HOOK_READ)
    static int data_r_fd = -1;
#endif // HOOK_READ

#if defined(__ANDROID__)
    #define DATA_PATH "/data/local/tmp"
#else // __ANDROID__
    #define DATA_PATH "/tmp"
#endif // __ANDROID__

#if defined(HOOK_WRITE)
    static const char *data_w_file = DATA_PATH "/write_data.bin";
#endif // HOOK_WRITE

#if defined(HOOK_READ)
    static const char *data_r_file = DATA_PATH "/read_data.bin";
#endif // HOOK_READ

static void _libhook_init() __attribute__ ((constructor));
static void _libhook_init() {
    /* causes segfault on some android, uncomment if you need it */
    //unsetenv("LD_PRELOAD");
    PRINT("Initialized");
}

ssize_t write (int fd, const void *buf, size_t count); //< Used only if DELIMITER is set.
void free (void *buf); //< Used only in read().

#if defined(HOOK_OPEN)
int open(const char* pathname, int flags, ...)
{
    static int (*func_open) (const char*, int, mode_t) = NULL;
    va_list args;
    mode_t mode;

    if (!func_open)
        func_open = (int (*) (const char*, int, mode_t)) dlsym (REAL_LIBC, "open");

    va_start (args, flags);
    mode = va_arg (args, int);
    va_end (args);

    // open() syscall.
    int fd = func_open(pathname, flags, mode);

    #if !defined(CONF_IOCTL_SPECIAL)
        if (!spy_file)
        {
            setenv("SPYFILE", "spyfile", 0);
            spy_file = getenv("SPYFILE");
        }
        if (strcmp(pathname, spy_file) != 0)
            return fd;
        hook_fd = fd;
        PRINT("open(\"%s\") -> %d", pathname, fd);
    #else // !CONF_IOCTL_SPECIAL
        hookAfterOpen(pathname, flags, mode, fd);
    #endif // !CONF_IOCTL_SPECIAL

    #if defined(HOOK_WRITE) || defined(HOOK_READ)
        #if defined(HOOK_WRITE)
            data_w_fd = func_open (data_w_file, O_WRONLY|O_CREAT|O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
        #endif // HOOK_WRITE
        #if defined(HOOK_READ)
            data_r_fd = func_open (data_r_file, O_WRONLY|O_CREAT|O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
        #endif // HOOK_READ

        /* write the delimiter each time we open the files */
        if (getenv("DELIMITER") != NULL)
        {
            #if defined(HOOK_READ)
                write (data_r_fd, getenv("DELIMITER"), strlen(getenv("DELIMITER")));
            #endif // HOOK_READ
            #if defined(HOOK_WRITE)
                write (data_w_fd, getenv("DELIMITER"), strlen(getenv("DELIMITER")));
            #endif // HOOK_WRITE
        }
    #endif // HOOK_WRITE || HOOK_READ

    return fd;
}
#endif // HOOK_OPEN

#if defined(HOOK_CLOSE)
int close(int fd)
{
    static int (*func_close)(int) = NULL;
    if (!func_close)
        func_close = (int (*)(int)) dlsym(REAL_LIBC, "close");

    int result = func_close(fd);

    #if !defined(CONF_IOCTL_SPECIAL)
        if (fd == hook_fd)
            PRINT("close(fd: %d \"%s\")", fd, spy_file);
    #else // !CONF_IOCTL_SPECIAL
        hookAfterClose(fd, result);
    #endif // !CONF_IOCTL_SPECIAL

    return result;
}
#endif // HOOK_STRCMP

#if defined(HOOK_IOCTL)
int ioctl(int fd, request_t request, ...)
{
    static IoctlFunc func_ioctl = NULL;
    if (!func_ioctl)
        func_ioctl = (IoctlFunc) dlsym(REAL_LIBC, "ioctl");

    va_list args;
    va_start (args, request);
    void* argp = va_arg(args, void*);
    va_end (args);

    #if !defined(CONF_IOCTL_SPECIAL)
        // ioctl() syscall.
        int result = func_ioctl(fd, request, argp);

        if (fd == hook_fd)
            OUTPUT("ioctl(fd: %d, ...) -> %d", fd, result);
    #else // !CONF_IOCTL_SPECIAL
        // Perform ioctl() syscall if and when needed.
        int result = hookHandleIoctl(fd, request, argp, func_ioctl);
    #endif // !CONF_IOCTL_SPECIAL

    return result;
}
#endif // HOOK_IOCTL

#if defined(HOOK_READ)
ssize_t read (int fd, void *buf, size_t count){

    static ssize_t (*func_read) (int, const void*, size_t) = NULL;
    static ssize_t (*func_write) (int, const void*, size_t) = NULL;

    ssize_t retval = 0;

    if (! func_read)
        func_read = (ssize_t (*) (int, const void*, size_t)) dlsym (REAL_LIBC, "read");
    if (! func_write)
        func_write = (ssize_t (*) (int, const void*, size_t)) dlsym (REAL_LIBC, "write");

    if (fd != hook_fd) {
        OUTPUT("read %zd bytes from file descriptor (fd=%d)", count, fd);
        return func_read (fd, buf, count);
    }

    OUTPUT("read %zd bytes from hooked file %s (fd=%d)", count, spy_file, fd);

    retval = func_read(fd, buf, count);

    char *buf2 = calloc(retval, sizeof(char));
    memcpy(buf2, buf, retval);

    func_write (data_r_fd, buf2, retval);
    free(buf2);

    return retval;
}
#endif // HOOK_READ

#if defined(HOOK_WRITE)
ssize_t write (int fd, const void *buf, size_t count){

    static ssize_t (*func_write) (int, const void*, size_t) = NULL;
    ssize_t retval = 0;

    if (! func_write)
        func_write = (ssize_t (*) (int, const void*, size_t)) dlsym (REAL_LIBC, "write");

    if (fd != hook_fd) {
        OUTPUT("write %zd bytes to file descriptor (fd=%d)", count, fd);
        return func_write (fd, buf, count);
    }

    OUTPUT("write %zd bytes to hooked file %s (fd=%d)", count, spy_file, fd);

    func_write (hook_fd, buf, count);
    retval = func_write (data_w_fd, buf, count);

    return retval;
}
#endif // HOOK_WRITE

#if defined(HOOK_STRCMP)
int strcmp(const char *s1, const char *s2) {

    static int (*func_strcmp) (const char *, const char *) = NULL;
    int retval = 0;

    if (! func_strcmp)
        func_strcmp = (int (*) (const char*, const char*)) dlsym (REAL_LIBC, "strcmp");

    if (getenv("SPYSTR") != NULL) {
        OUTPUT("strcmp( \"%s\" , \"%s\" )", s1, s2);
    }

    retval = func_strcmp (s1, s2);
    return retval;

}
#endif // HOOK_STRCMP

#if defined(HOOK_STRNCMP)
int strncmp(const char *s1, const char *s2, size_t n) {

    static int (*func_strncmp) (const char *, const char *, size_t) = NULL;
    int retval = 0;

    if (! func_strncmp)
        func_strncmp = (int (*) (const char*, const char*, size_t)) dlsym (REAL_LIBC, "strncmp");

    if (getenv("SPYSTR") != NULL) {
        OUTPUT("strncmp( \"%s\" , \"%s\" , %zd )", s1, s2, n);
    }

    retval = func_strncmp (s1, s2, n);
    return retval;

}
#endif // HOOK_STRCMP

#if defined(HOOK_FREE)
void free (void *ptr){

    static void (*func_free) (void*) = NULL;

    char *tmp = ptr;
    char tmp_buf[1025] = {0};
    size_t total = 0;

    if (! func_free)
        func_free = (void (*) (void*)) dlsym (REAL_LIBC, "free");

    if (getenv("SPYFREE") != NULL) {
        if (ptr != NULL) {

            while (*tmp != '\0') {
                tmp_buf[total] = *tmp;
                total++;
                if (total == 1024)
                    break;
                tmp++;
            }

            if (strlen(tmp_buf) != 0)
                OUTPUT("free( ptr[%zd]=%s )",strlen(tmp_buf), tmp_buf);
        }
    }

    func_free (ptr);
}
#endif // HOOK_FREE

#if defined(HOOK_MEMCPY)
void *memcpy(void *dest, const void *src, size_t n) {

    static void* (*func_memcpy) (void*, const void *, size_t) = NULL;
    if (! func_memcpy)
        func_memcpy = (void* (*) (void*, const void *, size_t)) dlsym (REAL_LIBC, "memcpy");

    OUTPUT("memcpy( dest=%p , src=%p, size=%zd )", dest, src, n);
    void* result = func_memcpy(dest,src,n);

    char *tmp = dest;
    char tmp_buf[1025] = {0};
    size_t total = 0;

    OUTPUT("    memcpy buffer: ");
    while (total < n) {
        tmp_buf[total] = *tmp;
        OUTPUT("%02X ", tmp_buf[total]);
        total++;
        if (total == 1024)
            break;
        tmp++;
    }

    OUTPUT("");
    OUTPUT("    memcpy str: [%zd]=%s )", strlen(tmp_buf), tmp_buf);
    return result;
}
#endif // HOOK_MEMCPY

#if defined(HOOK_PUTS)
int puts(const char *s) {

    static int (*func_puts) (const char *) = NULL;
    int retval = 0;

    if (! func_puts)
        func_puts = (int (*) (const char*)) dlsym (REAL_LIBC, "puts");

    OUTPUT("puts( \"%s\" )", s);

    retval = func_puts (s);
    return retval;
}
#endif // HOOK_PUTS

#if defined(HOOK_GETUID)
uid_t getuid(void) {

    static uid_t (*func_getuid) (void) = NULL;
    if (!func_getuid)
        func_getuid = (uid_t (*) (void)) dlsym (REAL_LIBC, "getuid");

    uid_t retval = func_getuid();
    OUTPUT("getuid returned %d", retval);

    return retval;
}
#endif // HOOK_GETUID

#if defined(HOOK_SYSTEM)
int system(const char *command) {

    static int (*func_system) (const char *) = NULL;
    int retval = 0;

    if (! func_system)
        func_system = (int (*) (const char*)) dlsym (REAL_LIBC, "system");

    retval = func_system (command);

    OUTPUT("system( \"%s\" ) returned %d", command, retval);

    return retval;

}
#endif // HOOK_SYSTEM

#if defined(HOOK_MALLOC)
void *malloc(size_t size) {

    static void* (*func_malloc) (size_t) = NULL;
    if (! func_malloc)
        func_malloc = (void* (*) (size_t)) dlsym (REAL_LIBC, "malloc");

    OUTPUT("malloc( size=%zd )", size);
    return func_malloc(size);
}
#endif // HOOK_MALLOC

#if defined(HOOK_ABORT)
void abort(void) {

    static void (*func_abort) (void) = NULL;
    if (! func_abort)
        func_abort = (void (*) (void)) dlsym (REAL_LIBC, "abort");
    OUTPUT("abort()");
    func_abort();
}
#endif // HOOK_ABORT

#if defined(HOOK_CHMOD)
int chmod(const char *path, mode_t mode) {

    static int (*func_chmod) (const char *, mode_t) = NULL;
    int retval = 0;

    if (! func_chmod)
        func_chmod = (int (*) (const char*, mode_t)) dlsym (REAL_LIBC, "chmod");

    retval = func_chmod (path, mode);

    OUTPUT("chmod( \"%s\", mode=%o ) returned %d", path, mode, retval);

    return retval;

}
#endif // HOOK_CHMOD

#if defined(HOOK_BSD_SIGNAL)
sighandler_t bsd_signal(int signum, sighandler_t handler) {

    static sighandler_t (*func_bsd_signal) (int, sighandler_t) = NULL;

    if (! func_bsd_signal)
        func_bsd_signal = (sighandler_t (*) (int, sighandler_t)) dlsym (REAL_LIBC, "bsd_signal");

    sighandler_t retval = func_bsd_signal (signum, handler);

    OUTPUT("bsd_signal \"%d\" ", signum);
    return retval;

}
#endif // HOOK_BSD_SIGNAL

#if defined(HOOK_UNLINK)
int unlink(const char *pathname) {
    static int (*func_unlink) (const char *) = NULL;
    int retval = 0;

    if (! func_unlink)
        func_unlink = (int (*) (const char*)) dlsym (REAL_LIBC, "unlink");

    retval = func_unlink (pathname);

    OUTPUT("unlink( \"%s\" ) returned %d", pathname, retval);

    return retval;

}
#endif // HOOK_UNLINK

#if defined(HOOK_FORK)
pid_t fork(void) {
    static pid_t (*func_fork) (void) = NULL;
    if (!func_fork)
        func_fork = (pid_t (*) (void)) dlsym (REAL_LIBC, "fork");

    pid_t retval = func_fork();
    OUTPUT("fork() returned %d", retval);

    return retval;
}
#endif // HOOK_FORK

#if defined(HOOK_SRAND48)
void srand48(long int seedval) {

    static void (*func_srand48) (long int) = NULL;
    if (! func_srand48)
        func_srand48 = (void (*) (long int)) dlsym (REAL_LIBC, "srand48");

    OUTPUT("srand48( size=%ld )", seedval);
    func_srand48(seedval);

}
#endif // HOOK_SRAND48

#if defined(HOOK_MEMSET)
void *memset(void *s, int c, size_t n) {

    static void* (*func_memset) (void*, int, size_t) = NULL;
    if (! func_memset)
        func_memset = (void* (*) (void*, int, size_t)) dlsym (REAL_LIBC, "memset");

    OUTPUT("memset( s=%p , c=%d, n=%zd )", s, c, n);
    return func_memset(s,c,n);
}
#endif // HOOK_MEMSET

#if defined(HOOK_TIME)
time_t time(time_t *t) {

    static time_t (*func_time) (time_t *) = NULL;
    time_t retval = 0;

    if (! func_time)
        func_time = (time_t (*) (time_t *)) dlsym (REAL_LIBC, "time");

    OUTPUT("time( \"%d\" )", (int) t);
    retval = func_time (t);
    return retval;
}
#endif // HOOK_TIME

#if defined(HOOK_LRAND48)
long int lrand48(void) {

    static long int (*func_lrand48) (void) = NULL;
    if (! func_lrand48)
        func_lrand48 = (long int (*) (void)) dlsym (REAL_LIBC, "lrand48");

    long int retval = func_lrand48();
    OUTPUT("lrand48() returned %ld", retval);

    return retval;
}
#endif // HOOK_LRAND48

#if defined(HOOK_STRLEN)
size_t strlen(const char *s) {

    static size_t (*func_strlen) (const char *) = NULL;
    int retval = 0;

    if (! func_strlen)
        func_strlen = (size_t (*) (const char*)) dlsym (REAL_LIBC, "strlen");

    retval = func_strlen (s);

    OUTPUT("strlen( \"%s\" ) returned %d", s, retval);

    return retval;
}
#endif // HOOK_STRLEN

#if defined(HOOK_RAISE)
int raise(int sig) {

    static int (*func_raise) (int) = NULL;
    int retval = 0;

    if (! func_raise)
        func_raise = (int (*) (int)) dlsym (REAL_LIBC, "raise");

    retval = func_raise (sig);
    OUTPUT("raise( \"%d\" ) returned %d", sig, retval);

    return retval;
}
#endif // HOOK_RAISE
