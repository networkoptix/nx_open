#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "../../system_commands.h"
#include "fd_send_linux.h"

namespace nx {
namespace system_commands {
namespace detail {

/*-----------------------------------------------------------------------------------------------*/

static int create_connected_socket(const char* path)
{
    struct sockaddr_un addr;
    int fd, tries = 0;
    static const int maxTries = 5;

    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
    {
        perror("create socket error");
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path)-1);

    while (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1)
    {
        if (tries > maxTries)
        {
            perror("connect error");
            return -1;
        }

        usleep(10 * 1000); /*< 10ms */
        tries++;
    }

    return fd;
}

/*-----------------------------------------------------------------------------------------------*/

static int sendFdImpl(int socketfd, int sendfd)
{
    struct msghdr msg;
    struct iovec iov[1];
    char buf[1];

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
    *((int *) CMSG_DATA(cmptr)) = sendfd;

    msg.msg_name = NULL;
    msg.msg_namelen = 0;

    iov[0].iov_base = buf;
    iov[0].iov_len = sizeof(buf);
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;

    return sendmsg(socketfd, &msg, 0);
}

/*-----------------------------------------------------------------------------------------------*/

bool sendFd(int fd)
{
    int socketfd = create_connected_socket(SystemCommands::kDomainSocket);
    if (socketfd < 0)
        return false;

    return sendFdImpl(socketfd, fd) > 0;
}

} // namespace detail
} // namespace system_commands
} // namespace nx

