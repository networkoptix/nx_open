#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "../../system_commands.h"
#include "send_linux.h"

namespace nx {
namespace system_commands {
namespace domain_socket {

struct DataContext
{
    void* data;
    ssize_t size;
};

int createConnectedSocket(const char* path)
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

    connect(fd, (struct sockaddr*)&addr, sizeof(addr));
    return fd;
}

bool sendFd(int transportFd, int fdToSend)
{
    struct msghdr msg;
    struct iovec iov[1];
    char buf[1];
    int result;

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

    ::close(fdToSend);
    return result > 0;
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

static bool sendImpl(int transportFd, const void* context, int (*action)(int, const void*))
{
    return action(transportFd, context) > 0;
}

bool sendInt64(int transportFd, int64_t value)
{
    struct DataContext context = {&value, sizeof(value)};
    return sendImpl(transportFd, &context, &sendData) > 0;
}

bool sendBuffer(int transportFd, const void* data, ssize_t size)
{
    struct DataContext context = {(void*) data, size};
    return sendImpl(transportFd, &context, &sendData) > 0;
}

} // namespace domain_socket
} // namespace system_commands
} // namespace nx

