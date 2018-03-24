#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "../system_commands.h"
#include "fd_read_linux.h"

namespace nx {
namespace system_commands {

static ssize_t readFdImpl(int fd, int *recvfd)
{
    struct msghdr msg;
    struct iovec iov[1];
    ssize_t n;
    char buf[1];

    union {
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

    if ((n = recvmsg(fd, &msg, 0)) <= 0)
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
        *recvfd = *((int *) CMSG_DATA(cmptr));
    }
    else
    {
        *recvfd = -1;
    }

    return n;
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

    return accept(fd, NULL, NULL);
}

/*-----------------------------------------------------------------------------------------------*/

int readFd()
{
    int acceptfd = acceptConnection(SystemCommands::kDomainSocket);
    if (acceptfd < 0)
        return -1;

    int recvfd;
    if (readFdImpl(acceptfd, &recvfd) <= 0)
        return -1;

    return recvfd;
}

} // namespace system_commands
} // namespace nx
