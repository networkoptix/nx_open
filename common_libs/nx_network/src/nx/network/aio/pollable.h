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

#ifndef _WIN32
static const int INVALID_SOCKET = -1;
#endif

//!Incapsulates system object that can be polled with \a PollSet
class NX_NETWORK_API Pollable
{
public:
    /*!
        \param fd Valid file descriptor. It is not closed on object destruction!
    */
    Pollable(
        AbstractSocket::SOCKET_HANDLE fd,
        std::unique_ptr<CommonSocketImpl> impl = std::unique_ptr<CommonSocketImpl>() );

    Pollable(const Pollable&) = delete;
    Pollable& operator=(const Pollable&) = delete;
    Pollable(Pollable&&) = delete;
    Pollable& operator=(Pollable&&) = delete;

    virtual ~Pollable() {}

    AbstractSocket::SOCKET_HANDLE handle() const;
    /** Moves ownership pf system socket out of \a Socket instance.
        Leaves \a Socket instance in undefined state.
        \note Caller MUST ensure that there are no async socket operations on this instance
    */
    AbstractSocket::SOCKET_HANDLE takeHandle();
    /*!
        \note Zero timeout means infinite timeout
    */
    bool getRecvTimeout( unsigned int* millis ) const;
    /*!
        \note Zero timeout means infinite timeout
    */
    bool getSendTimeout( unsigned int* millis ) const;

    CommonSocketImpl* impl();
    const CommonSocketImpl* impl() const;

    virtual bool getLastError( SystemError::ErrorCode* errorCode ) const;

    nx::network::aio::AbstractAioThread* getAioThread() const;
    void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread);
    bool isInSelfAioThread() const;

protected:
    AbstractSocket::SOCKET_HANDLE m_fd;
    std::unique_ptr<CommonSocketImpl> m_impl;
    unsigned int m_readTimeoutMS;
    unsigned int m_writeTimeoutMS;
};

}   //network
}   //nx

#endif  //NX_POLLABLE_H
