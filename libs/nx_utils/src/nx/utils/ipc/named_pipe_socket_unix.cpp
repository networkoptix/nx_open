// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "named_pipe_socket.h"

#include <poll.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <chrono>

#include <nx/utils/time.h>

#include "named_pipe_socket_unix.h"

namespace nx::utils {

NamedPipeSocket::NamedPipeSocket():
    m_impl(new NamedPipeSocketImpl())
{
}

NamedPipeSocket::~NamedPipeSocket()
{
    if (m_impl->hPipe >= 0)
    {
        ::close(m_impl->hPipe);
        m_impl->hPipe = -1;
    }

    delete m_impl;
    m_impl = NULL;
}

SystemError::ErrorCode NamedPipeSocket::connectToServerSync(
    const std::string& pipeName,
    int /*timeoutMillis*/)
{
    if (m_impl->hPipe >= 0)
        ::close(m_impl->hPipe);

    // TODO: #akolesnikov timeoutMillis support.

    m_impl->hPipe = socket(AF_UNIX, SOCK_STREAM, 0);
    if (m_impl->hPipe < 0)
        return errno;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "/tmp/%s", pipeName.c_str());
    if (connect(m_impl->hPipe, (sockaddr*)& addr, sizeof(addr)) != 0)
    {
        const int connectErrorCode = errno;
        ::close(m_impl->hPipe);
        m_impl->hPipe = -1;
        return connectErrorCode;
    }

    return SystemError::noError;
}

SystemError::ErrorCode NamedPipeSocket::write(
    const void* buf,
    unsigned int bytesToWrite,
    unsigned int* const bytesWritten)
{
    *bytesWritten = 0;
    while (*bytesWritten < bytesToWrite)
    {
        ssize_t numberOfBytesWritten = ::write(
            m_impl->hPipe,
            (const char*)buf + *bytesWritten,
            bytesToWrite - *bytesWritten);

        if (numberOfBytesWritten < 0)
        {
            const int errCode = errno;
            if (errCode == EINTR)
                continue;
            return errCode;
        }
        *bytesWritten += numberOfBytesWritten;
    }

    return SystemError::noError;
}

SystemError::ErrorCode NamedPipeSocket::read(
    void* buf,
    unsigned int bytesToRead,
    unsigned int* const bytesRead,
    const int timeoutMs)
{
    if (m_impl->hPipe < 0)
        return SystemError::badDescriptor;

    using namespace std::chrono;

    const auto deadline = monotonicTime() + milliseconds(timeoutMs);

    for (;;)
    {
        int remainingTimeoutMs = -1; //< Negative timeout means infinite wait for poll().
        if (timeoutMs >= 0)
        {
            const auto remainingMs = ceil<milliseconds>(deadline - monotonicTime()).count();
            remainingTimeoutMs = remainingMs > 0 ? (int)remainingMs : 0;
        }

        struct pollfd pollDescriptor { m_impl->hPipe, POLLIN, 0 };
        ssize_t result = ::poll(&pollDescriptor, 1, remainingTimeoutMs);

        if (result == 0)
            return SystemError::timedOut;

        if (result > 0)
            result = ::read(m_impl->hPipe, buf, bytesToRead);

        if (result < 0)
        {
            const int errCode = errno;
            if (errCode == EINTR)
                continue;
            return errCode;
        }

        *bytesRead = (unsigned int)result;
        return SystemError::noError;
    }
}

SystemError::ErrorCode NamedPipeSocket::flush()
{
    // TODO: #akolesnikov
    return SystemError::noError;
}

NamedPipeSocket::NamedPipeSocket(NamedPipeSocketImpl* implToUse):
    m_impl(implToUse)
{
}

} // namespace nx::utils
