/**********************************************************
* 29 oct 2014
* a.kolesnikov
***********************************************************/

#ifndef NX_POLLABLE_H
#define NX_POLLABLE_H

#include <memory>

#include "../abstract_socket.h"
#include "../common_socket_impl.h"


class Pollable;
namespace aio
{
    extern template class AIOThread<Pollable>;
}

#ifndef _WIN32
static const int INVALID_SOCKET = -1;
#endif

typedef CommonSocketImpl<Pollable> PollableImpl;

class Pollable
{
public:
    Pollable(
        AbstractSocket::SOCKET_HANDLE fd,
        std::unique_ptr<PollableImpl> impl = std::unique_ptr<PollableImpl>() );
    virtual ~Pollable() {}

    AbstractSocket::SOCKET_HANDLE handle() const;
    bool getRecvTimeout( unsigned int* millis );
    bool getSendTimeout( unsigned int* millis );

    PollableImpl* impl();
    const PollableImpl* impl() const;

    virtual bool getLastError( SystemError::ErrorCode* errorCode ) const = 0;

protected:
    AbstractSocket::SOCKET_HANDLE m_fd;
    std::unique_ptr<PollableImpl> m_impl;
    unsigned int m_readTimeoutMS;
    unsigned int m_writeTimeoutMS;
};

#endif  //NX_POLLABLE_H
