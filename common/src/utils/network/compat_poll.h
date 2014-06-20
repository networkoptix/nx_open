/**********************************************************
* 22 may 2014
* a.kolesnikov
***********************************************************/

#ifndef NX_COMPAT_POLL_H
#define NX_COMPAT_POLL_H


#if WINVER >= 0x0600

#  include <Winsock2.h>
#  define poll WSAPoll

#elif WINVER >= 0x0500

#  error "poll not implemented yet. Ask Andrey"

#elif !defined(_WIN32)

#  include <poll.h>

#endif

#endif  //NX_COMPAT_POLL_H
