// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "named_pipe_server.h"

#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "named_pipe_socket_unix.h"

namespace nx::utils {

class NamedPipeServerImpl
{
public:
    int hPipe = -1;
};

//-------------------------------------------------------------------------------------------------
// class NamedPipeServer

NamedPipeServer::NamedPipeServer():
    m_impl(new NamedPipeServerImpl())
{
}

NamedPipeServer::~NamedPipeServer()
{
    if (m_impl->hPipe >= 0)
    {
        ::close(m_impl->hPipe);
        m_impl->hPipe = -1;
    }

    delete m_impl;
    m_impl = NULL;
}

SystemError::ErrorCode NamedPipeServer::listen(const std::string& pipeName)
{
    static const int BACKLOG_SIZE = 7;

    m_impl->hPipe = socket(AF_UNIX, SOCK_STREAM, 0);
    if (m_impl->hPipe < 0)
        return errno;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "/tmp/%s", pipeName.c_str());
    if (::bind(m_impl->hPipe, (sockaddr*)& addr, sizeof(addr)) != 0 ||
        ::listen(m_impl->hPipe, BACKLOG_SIZE) != 0)
    {
        const int connectErrorCode = errno;
        ::close(m_impl->hPipe);
        m_impl->hPipe = -1;
        return connectErrorCode;
    }

    return SystemError::noError;
}

SystemError::ErrorCode NamedPipeServer::accept(NamedPipeSocket** sock, int /*timeoutMillis*/)
{
    // TODO: #akolesnikov timeoutMillis support

    for (;;)
    {
        const int newSockDesc = ::accept(m_impl->hPipe, NULL, NULL);
        if (newSockDesc < 0)
        {
            const int errCode = errno;
            if (errCode == EINTR)
                continue;
            return errCode;
        }
        *sock = new NamedPipeSocket();
        (*sock)->m_impl->hPipe = newSockDesc;
        return SystemError::noError;
    }
}

} // namespace nx::utils
