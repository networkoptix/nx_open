#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/poll.h>
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
        return -1;

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

static ssize_t readImpl(int fd, void* context, ssize_t (*action)(int, void*))
{
    return action(fd, context);
}

int readFd(int fd)
{
    int recvFd;
    if (readImpl(fd, &recvFd, &readFdImpl) < 0)
        return -1;

    return recvFd;
}

bool readInt64(int fd, int64_t* value)
{
    struct DataContext context = {value, NULL, NULL, sizeof(*value)};
    return readImpl(fd, &context, &readData) > 0;
}

bool readBuffer(int fd, void* (*reallocCallback)(void*, ssize_t), void* reallocContext)
{
    struct DataContext context = {NULL, reallocCallback, reallocContext, 0};
    return readImpl(fd, &context, &readData) > 0;
}

} // namespace domain_socket
} // namespace system_commands
} // namespace nx
