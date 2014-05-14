////////////////////////////////////////////////////////////
// 14 jan 2014    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef _WIN32

#include "named_pipe_server.h"

#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "named_pipe_socket_unix.h"


////////////////////////////////////////////////////////////
//// class NamedPipeServerImpl
////////////////////////////////////////////////////////////
class NamedPipeServerImpl
{
public:
    int hPipe;

    NamedPipeServerImpl()
    :
        hPipe( -1 )
    {
    }
};

////////////////////////////////////////////////////////////
//// class NamedPipeServer
////////////////////////////////////////////////////////////
NamedPipeServer::NamedPipeServer()
:
    m_impl( new NamedPipeServerImpl() )
{
}

NamedPipeServer::~NamedPipeServer()
{
    if( m_impl->hPipe >= 0 )
    {
        ::close( m_impl->hPipe );
        m_impl->hPipe = -1;
    }

    delete m_impl;
    m_impl = NULL;
}

//!Openes \a pipeName and listenes it for connections
/*!
    This method returns immediately
*/
SystemError::ErrorCode NamedPipeServer::listen( const QString& pipeName )
{
    static const int BACKLOG_SIZE = 7;

    m_impl->hPipe = socket( AF_UNIX, SOCK_STREAM, 0 );
    if( m_impl->hPipe < 0 )
        return errno;

    struct sockaddr_un addr;
    memset( &addr, 0, sizeof(addr) );
    addr.sun_family = AF_UNIX;
    sprintf( addr.sun_path, "/tmp/%s", pipeName.toLatin1().constData() );
    if( ::bind( m_impl->hPipe, (sockaddr*)&addr, sizeof(addr) ) != 0 ||
        ::listen( m_impl->hPipe, BACKLOG_SIZE ) != 0 )
    {
        const int connectErrorCode = errno;
        ::close( m_impl->hPipe );
        m_impl->hPipe = -1;
        return connectErrorCode;
    }

    return SystemError::noError;
}

/*!
    Blocks till new connection is accepted, timeout expires or error occurs
    \param timeoutMillis max time to wait for incoming connection (millis)
    \param sock If return value \a SystemError::noError, \a *sock is assigned with newly-created socket. It must be freed by calling party
*/
SystemError::ErrorCode NamedPipeServer::accept( NamedPipeSocket** sock, int /*timeoutMillis*/ )
{
    //TODO/IMPL: #ak timeoutMillis support

    for( ;; )
    {
        const int newSockDesc = ::accept( m_impl->hPipe, NULL, NULL );
        if( newSockDesc < 0 )
        {
            const int errCode = errno;
            if( errCode == EINTR )
                continue;
            return errCode;
        }
        *sock = new NamedPipeSocket();
        (*sock)->m_impl->hPipe = newSockDesc;
        return SystemError::noError;
    }
}

#endif
