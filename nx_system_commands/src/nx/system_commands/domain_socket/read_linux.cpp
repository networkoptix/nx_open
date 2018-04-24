#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/select.h>
#include <fcntl.h>
#include "../../system_commands.h"
#include "read_linux.h"

namespace nx {
namespace system_commands {
namespace domain_socket {

struct DataContext
{
    void* data;
    void* (*reallocCallback)(void*, ssize_t);
    void* callbackContext;
    ssize_t size;
};

static const int kMaximumAcceptTimeoutSec = 3;

static int acceptWithTimeout(int fd, int timeoutSec)
{
   int result;
   struct timeval tv;
   fd_set rfds;
   FD_ZERO(&rfds);
   FD_SET(fd, &rfds);

   tv.tv_sec = (long)timeoutSec;
   tv.tv_usec = 0;

   result = select(fd + 1, &rfds, (fd_set*) 0, (fd_set*) 0, &tv);
   if(result > 0)
       return accept(fd, NULL, NULL);

   return -1;
}

static int acceptConnection(const char* path)
{
    struct sockaddr_un addr;
    int fd;

    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
    {
        perror("socket error");
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path)-1);

    unlink(path);
    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1)
    {
        perror("bind error");
        return -1;
    }

    if (listen(fd, 5) == -1)
    {
        perror("listen error");
        return -1;
    }

    int acceptedFd = acceptWithTimeout(fd, kMaximumAcceptTimeoutSec);
    ::close(fd);

    return acceptedFd;
}

static ssize_t readFdImpl(int transportFd, void* context)
{
    struct msghdr msg;
    struct iovec iov[1];
    ssize_t n;
    char buf[1];

    union
    {
        struct cmsghdr cm;
        char control[CMSG_SPACE(sizeof(int))];
    } control_un;
    struct cmsghdr *cmptr;

    msg.msg_control = control_un.control;
    msg.msg_controllen = sizeof(control_un.control);

    msg.msg_name = NULL;
    msg.msg_namelen = 0;

    iov[0].iov_base = buf;
    iov[0].iov_len = sizeof(buf);
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;

    if ((n = recvmsg(transportFd, &msg, 0)) <= 0)
        return n;

    if ((cmptr = CMSG_FIRSTHDR(&msg)) != NULL && cmptr->cmsg_len == CMSG_LEN(sizeof(int)))
    {
        if (cmptr->cmsg_level != SOL_SOCKET)
        {
            perror("control level != SOL_SOCKET");
            return -1;
        }
        if (cmptr->cmsg_type != SCM_RIGHTS)
        {
            perror("control type != SCM_RIGHTS");
            return -1;
        }
        *((int*)context) = *((int *) CMSG_DATA(cmptr));
    }
    else
    {
        *((int*)context) = -1;
    }

    return n;
}

static ssize_t readData(int transportFd, void* context)
{
    struct DataContext* dataContext = (struct DataContext*) context;
    ssize_t messageSize, readBytes, total = 0;

    if (read(transportFd, &messageSize, sizeof(messageSize)) <= 0)
        return -1;

    if (dataContext->size < messageSize)
    {
        dataContext->data = dataContext->reallocCallback(dataContext->callbackContext, messageSize);
        if (dataContext->data == NULL)
            return -1;
    }

    while (total < messageSize)
    {
        readBytes = read(transportFd, (char*) dataContext->data + total, messageSize - total);
        if (readBytes <= 0)
            return -1;

        total += readBytes;
    }

    return total;
}

static ssize_t readImpl(void* context, ssize_t (*action)(int, void*))
{
    ssize_t result = -1;
    int transportFd = acceptConnection(SystemCommands::kDomainSocket);
    if (transportFd > 0)
    {
        result = action(transportFd, context);
        ::close(transportFd);
    }
    unlink(SystemCommands::kDomainSocket);

    return result;
}

int readFd()
{
    int recvFd;
    if (readImpl(&recvFd, &readFdImpl) < 0)
        return -1;

    return recvFd;
}

bool readInt64(int64_t* value)
{
    struct DataContext context = {value, NULL, NULL, sizeof(*value)};
    return readImpl(&context, &readData) > 0;
}

bool readBuffer(void* (*reallocCallback)(void*, ssize_t), void* reallocContext)
{
    struct DataContext context = {NULL, reallocCallback, reallocContext, 0};
    return readImpl(&context, &readData) > 0;
}

} // namespace domain_socket
} // namespace system_commands
} // namespace nx
