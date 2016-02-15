/**********************************************************
* 29 oct 2014
* a.kolesnikov
***********************************************************/

#ifndef NX_POLLABLE_H
#define NX_POLLABLE_H

#include <memory>

#include "../abstract_socket.h"
#include "../common_socket_impl.h"


namespace nx {
namespace network {

class Pollable;
class AbstractAioThread;

#ifndef _WIN32
static const int INVALID_SOCKET = -1;
#endif

typedef CommonSocketImpl<Pollable> PollableImpl;

//!Incapsulates system object that can be polled with \a PollSet
class NX_NETWORK_API Pollable
{
public:
    /*!
        \param fd Valid file descriptor. It is not closed on object destruction!
    */
    Pollable(
        AbstractSocket::SOCKET_HANDLE fd,
        std::unique_ptr<PollableImpl> impl = std::unique_ptr<PollableImpl>() );
    virtual ~Pollable() {}

    Pollable(const Pollable&) = delete;
    Pollable& operator=(const Pollable&) = delete;
    Pollable(Pollable&&) = default;
    Pollable& operator=(Pollable&&) = default;

    AbstractSocket::SOCKET_HANDLE handle() const;
    /*!
        \note Zero timeout means infinite timeout
    */
    bool getRecvTimeout( unsigned int* millis ) const;
    /*!
        \note Zero timeout means infinite timeout
    */
    bool getSendTimeout( unsigned int* millis ) const;

    PollableImpl* impl();
    const PollableImpl* impl() const;

    virtual bool getLastError( SystemError::ErrorCode* errorCode ) const;

    nx::network::aio::AbstractAioThread* getAioThread();
    void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread);

protected:
    AbstractSocket::SOCKET_HANDLE m_fd;
    std::unique_ptr<PollableImpl> m_impl;
    unsigned int m_readTimeoutMS;
    unsigned int m_writeTimeoutMS;
};

}   //network
}   //nx

#endif  //NX_POLLABLE_H
