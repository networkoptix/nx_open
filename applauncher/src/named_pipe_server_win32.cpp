////////////////////////////////////////////////////////////
// 18 aug 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "named_pipe_server.h"

#include "named_pipe_socket_win32.h"


#ifdef _WIN32

////////////////////////////////////////////////////////////
//// class NamedPipeServerImpl
////////////////////////////////////////////////////////////
class NamedPipeServerImpl
{
public:
    HANDLE hPipe;

    NamedPipeServerImpl()
    :
        hPipe( INVALID_HANDLE_VALUE )
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
    CloseHandle( m_impl->hPipe );
    m_impl->hPipe = INVALID_HANDLE_VALUE;

    delete m_impl;
    m_impl = NULL;
}

//!Openes \a pipeName and listenes it for connections
/*!
    This method returns immediately
*/
SystemError::ErrorCode NamedPipeServer::listen( const QString& pipeName )
{
    const QString win32PipeName = QString::fromLatin1("\\\\.\\pipe\\%1").arg(pipeName);
    m_impl->hPipe = CreateNamedPipe(
        win32PipeName.utf16(),             // pipe name 
        FILE_FLAG_FIRST_PIPE_INSTANCE | PIPE_ACCESS_DUPLEX,       // read/write access 
        PIPE_TYPE_MESSAGE |       // message type pipe 
            PIPE_READMODE_MESSAGE |   // message-read mode 
            PIPE_WAIT,                // blocking mode 
        PIPE_UNLIMITED_INSTANCES, // max. instances  
        NamedPipeSocketImpl::BUFSIZE,      // output buffer size 
        NamedPipeSocketImpl::BUFSIZE,      // input buffer size 
        0,                        // client time-out 
        NULL );                    // default security attribute 

    return m_impl->hPipe != INVALID_HANDLE_VALUE
        ? SystemError::noError
        : SystemError::getLastOSErrorCode();
}

/*!
    Blocks till new connection is accepted, timeout expires or error occurs
    \param timeoutMillis max time to wait for incoming connection (millis)
    \param sock If return value \a SystemError::noError, \a *sock is assigned with newly-created socket. It must be freed by calling party
*/
SystemError::ErrorCode NamedPipeServer::accept( NamedPipeSocket** sock, int /*timeoutMillis*/ )
{
    //TODO/IMPL: #ak timeoutMillis support

    if( !ConnectNamedPipe( m_impl->hPipe, NULL ) )
        return SystemError::getLastOSErrorCode();

    NamedPipeSocketImpl* sockImpl = new NamedPipeSocketImpl();
    sockImpl->hPipe = m_impl->hPipe;
    *sock = new NamedPipeSocket( sockImpl );

    return SystemError::noError;
}

#endif
