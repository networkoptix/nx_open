#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "../../../system_commands.h"
#include "send_linux.h"

namespace nx {
namespace system_commands {
namespace domain_socket {
namespace detail {

struct DataContext
{
    void* data;
    ssize_t size;
};

static int createConnectedSocket(const char* path)
{
    struct sockaddr_un addr;
    int fd;

    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
    {
        perror("create socket error");
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path)-1);

    while (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1)
        usleep(10 * 1000); /*< 10ms */

    return fd;
}

static int sendFdImpl(int transportFd, const void* data)
{
    struct msghdr msg;
    struct iovec iov[1];
    char buf[1];
    int fdToSend = (intptr_t) data, result;

    union
    {
        struct cmsghdr cm;
        char control[CMSG_SPACE(sizeof(int))];
    } control_un;
    struct cmsghdr  *cmptr;

    msg.msg_control = control_un.control;
    msg.msg_controllen = sizeof(control_un.control);

    cmptr = CMSG_FIRSTHDR(&msg);
    cmptr->cmsg_len = CMSG_LEN(sizeof(int));
    cmptr->cmsg_level = SOL_SOCKET;
    cmptr->cmsg_type = SCM_RIGHTS;
    *((int *) CMSG_DATA(cmptr)) = fdToSend;

    msg.msg_name = nullptr;
    msg.msg_namelen = 0;

    iov[0].iov_base = buf;
    iov[0].iov_len = sizeof(buf);
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;

    result = sendmsg(transportFd, &msg, 0);
    if (result < 0)
        perror("sendmsg");

    return result;
}

static int sendData(int transportFd, const void* context)
{
    struct DataContext* dataContext = (struct DataContext*) context;
    ssize_t written, total = 0;

    if (write(transportFd, &dataContext->size, sizeof(dataContext->size)) <= 0)
    {
        perror("sendData");
        return -1;
    }

    while (total < dataContext->size)
    {
        written = write(transportFd, (char*) dataContext->data + total,
            dataContext->size - total);

        if (written <= 0)
        {
            perror("write");
            return -1;
        }

        total += written;
    }

    return total;
}

static bool sendImpl(int socketPostfix, const void* context, int (*action)(int, const void*))
{
    assert(socketPostfix != -1);
    static const int baseLen = strlen(SystemCommands::kDomainSocket);
    char buf[512];

    strncpy(buf, SystemCommands::kDomainSocket, sizeof(buf));
    snprintf(buf + baseLen, sizeof(buf) - baseLen, "%d", socketPostfix);

    int transportFd = createConnectedSocket(buf);
    if (transportFd < 0)
        return false;

    bool result = action(transportFd, context) > 0;
    ::close(transportFd);

    return result;
}

bool sendFd(int socketPostfix, int fd)
{
    return sendImpl(socketPostfix, (const void*) fd, &sendFdImpl) > 0;
}

bool sendInt64(int socketPostfix, int64_t value)
{
    struct DataContext context = {&value, sizeof(value)};
    return sendImpl(socketPostfix, &context, &sendData) > 0;
}

bool sendBuffer(int socketPostfix, const void* data, ssize_t size)
{
    struct DataContext context = {(void*) data, size};
    return sendImpl(socketPostfix, &context, &sendData) > 0;
}

} // namespace detail
} // namespace domain_socket
} // namespace system_commands
} // namespace nx

