/**********************************************************
* 22 may 2014
* a.kolesnikov
***********************************************************/

#ifndef NX_COMPAT_POLL_H
#define NX_COMPAT_POLL_H

#include "version.h"


#ifdef _WIN32

#include <Winsock2.h>

#if WINVER < 0x0600

struct nx_pollfd
{
    SOCKET  fd;
    SHORT   events;
    SHORT   revents;
};
typedef nx_pollfd pollfd;

#define POLLRDNORM  0x0100
#define POLLRDBAND  0x0200
#define POLLIN      (POLLRDNORM | POLLRDBAND)
#define POLLPRI     0x0400

#define POLLWRNORM  0x0010
#define POLLOUT     (POLLWRNORM)
#define POLLWRBAND  0x0020

#define POLLERR     0x0001
#define POLLHUP     0x0002
#define POLLNVAL    0x0004

#endif

typedef int (*PollFuncType)( pollfd /*fdarray*/[], ULONG /*nfds*/, INT /*timeout*/ );
PollFuncType getPollFuncAddress();

#define poll getPollFuncAddress()

#else   //unix

#  include <poll.h>

#endif

#endif  //NX_COMPAT_POLL_H
