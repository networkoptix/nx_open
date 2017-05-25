#include "handle_fb_disp.h"

#include <string.h>
#include <stdint.h>

#include <pthread.h>

#include "dev_fb.h"

static pthread_mutex_t lock;

static int performIoctl(
    const char* filename, int fd, request_t request, void* argp, IoctlFunc func_ioctl,
    bool* outDelayed)
{
    /*unused*/ (void) filename;

    // ATTENTION: This code may deadlock if a signal handler will call ioctl() within this thread.
    if (pthread_mutex_trylock(&lock) != 0)
    {
        // Mutex was already locked by another thread.
        *outDelayed = true;
        pthread_mutex_lock(&lock); //< Wait for another thread to unlock.
    }
    // Mutex is now locked by either trylock() or lock().

    int result = func_ioctl(fd, request, argp);

    pthread_mutex_unlock(&lock);
    return result;
}

//-------------------------------------------------------------------------------------------------

typedef enum _FileType
{
    FileType_FB,
    FileType_DISP,
} FileType;

typedef struct _File
{
    const char* const filename;
    const FileType type;
    int fd;

} File;

static File files[] =
{
    {
        .filename = "/dev/fb0",
        .type = FileType_FB,
        .fd = -1,
    },
    {
        .filename = "/dev/disp",
        .type = FileType_DISP,
        .fd = -1,
    },
};

static const int filesCount = sizeof(files) / sizeof(files[0]);

void hookAfterOpen(const char* pathname, int flags, mode_t mode, int res)
{
    /*unused*/ (void) flags;
    /*unused*/ (void) mode;

    if (res < 0) //< open() failed, ignore the call.
        return;

    // Store fd of the opened file in files[].
    for (int i = 0; i < filesCount; ++i)
    {
        if (strcmp(pathname, files[i].filename) == 0)
        {
            files[i].fd = res;
            PRINT("HOOKED: open(\"%s\") -> fd: %d", pathname, res);
            return;
        }
    }

    // Not found in files[].
    OUTPUT("IGNORED: open(\"%s\") -> fd: %d", pathname, res);
}

void hookAfterClose(int fd, int res)
{
    if (res < 0) //< close() failed, ignore the call.
        return;

    // Delete fd of the closed file from files[].
    for (int i = 0; i < filesCount; ++i)
    {
        if (files[i].fd == fd)
        {
            files[i].fd = -1;
            PRINT("HOOKED: close(fd: %d \"%s\") -> %d", fd, files[i].filename, res);
            return;
        }
    }

    // Not found in files[].
    OUTPUT("IGNORED: close(fd: %d) -> %d", fd, res);
}

int hookHandleIoctl(int fd, request_t request, void* argp, IoctlFunc func_ioctl)
{
    // Search fd in files[].
    for (int i = 0; i < filesCount; ++i)
    {
        if (files[i].fd == fd)
        {
            int result = -1;
            bool delayed = false;
            if (conf.serializeFbAndDisp)
                result = performIoctl(files[i].filename, fd, request, argp, func_ioctl, &delayed);
            else
                result = func_ioctl(fd, request, argp);

            // Log hooked ioctl() only once, to avoid flooding.
            {
                static bool once = false;
                if (!once || delayed)
                {
                    once = true;
                    const char* const prefix = delayed ? "DELAYED" : "HOOKED";
                    switch (files[i].type)
                    {
                        case FileType_FB:
                            hookPrintIoctlDevFb(prefix,
                                files[i].filename, fd, request, argp, result);
                            break;
                        case FileType_DISP:
                            PRINT("%s: ioctl(fd: %d \"%s\", 0x%X, 0x%X) -> %d", prefix,
                                fd, files[i].filename, (uint32_t) request, (uint32_t) argp, result);
                            break;
                        default:
                        {
                        }
                    }
                }
            }

            return result;
        }
    }

    // Not found in files[] - perform regular ioctl().
    int result = func_ioctl(fd, request, argp);
    OUTPUT("IGNORED: ioctl(fd: %d, request: 0x%X, argp: 0x%X) -> %d",
        fd, (uint32_t) request, (uint32_t) argp, res);
    return result;
}
