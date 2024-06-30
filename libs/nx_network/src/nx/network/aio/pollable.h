// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include "../abstract_socket.h"

namespace nx::network {

#ifndef _WIN32
static const int INVALID_SOCKET = -1;
#endif

class CommonSocketImpl;

/**
 * Encapsulates system object (socket, pipe, event, ...) that can be polled with PollSet.
 * Each object is always bound to an AIO thread which polls for events.
 */
class NX_NETWORK_API Pollable
{
public:
    /**
     * @param aioThread to bind to. Null is allowed here.
     * @param fd File descriptor (optional, can be INVALID_SOCKET).
     * It is not closed on object destruction!
     */
    Pollable(
        aio::AbstractAioThread* aioThread,
        AbstractSocket::SOCKET_HANDLE fd,
        std::unique_ptr<CommonSocketImpl> impl);

    Pollable(
        aio::AbstractAioThread* aioThread,
        AbstractSocket::SOCKET_HANDLE fd);

    Pollable(const Pollable&) = delete;
    Pollable& operator=(const Pollable&) = delete;
    Pollable(Pollable&&) = delete;
    Pollable& operator=(Pollable&&) = delete;

    virtual ~Pollable();

    AbstractSocket::SOCKET_HANDLE handle() const;

    /**
     * Moves ownership of system socket handle out of this object.
     * Leaves this with handler equal to INVALID_SOCKET.
     * NOTE: Caller MUST ensure that there are no async socket operations on this instance.
     */
    AbstractSocket::SOCKET_HANDLE takeHandle();

    /**
     * NOTE: Zero timeout means infinite timeout.
     */
    bool getRecvTimeout(unsigned int* millis) const;

    /**
     * NOTE: Zero timeout means infinite timeout.
     */
    bool getSendTimeout(unsigned int* millis) const;

    CommonSocketImpl* impl();
    const CommonSocketImpl* impl() const;

    virtual bool getLastError(SystemError::ErrorCode* errorCode) const;

    nx::network::aio::AbstractAioThread* getAioThread() const;
    void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread);
    bool isInSelfAioThread() const;

protected:
    AbstractSocket::SOCKET_HANDLE m_fd = INVALID_SOCKET;
    unsigned int m_readTimeoutMS = 0;
    unsigned int m_writeTimeoutMS = 0;

private:
    std::unique_ptr<CommonSocketImpl> m_impl;
};

} // namespace nx::network
