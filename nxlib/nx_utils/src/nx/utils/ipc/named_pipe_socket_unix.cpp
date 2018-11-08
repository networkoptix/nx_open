#include "named_pipe_socket.h"

#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "named_pipe_socket_unix.h"


////////////////////////////////////////////////////////////
//// class NamedPipeSocketImpl
////////////////////////////////////////////////////////////
NamedPipeSocketImpl::NamedPipeSocketImpl()
:
    hPipe( -1 )
{
}


////////////////////////////////////////////////////////////
//// class NamedPipeSocket
////////////////////////////////////////////////////////////
NamedPipeSocket::NamedPipeSocket()
:
    m_impl( new NamedPipeSocketImpl() )
{
}

NamedPipeSocket::~NamedPipeSocket()
{
    if( m_impl->hPipe >= 0 )
    {
        ::close( m_impl->hPipe );
        m_impl->hPipe = -1;
    }

    delete m_impl;
    m_impl = NULL;
}

//!
/*!
    Blocks till connection is complete
    \param timeoutMillis if -1, can wait indefinetely
*/
SystemError::ErrorCode NamedPipeSocket::connectToServerSync( const QString& pipeName, int /*timeoutMillis*/ )
{
    if( m_impl->hPipe >= 0 )
        ::close( m_impl->hPipe );

    //TODO/IMPL: #ak timeoutMillis support

    m_impl->hPipe = socket( AF_UNIX, SOCK_STREAM, 0 );
    if( m_impl->hPipe < 0 )
        return errno;

    struct sockaddr_un addr;
    memset( &addr, 0, sizeof(addr) );
    addr.sun_family = AF_UNIX;
    sprintf( addr.sun_path, "/tmp/%s", pipeName.toLatin1().constData() );
    if( connect( m_impl->hPipe, (sockaddr*)&addr, sizeof(addr) ) != 0 )
    {
        const int connectErrorCode = errno;
        ::close( m_impl->hPipe );
        m_impl->hPipe = -1;
        return connectErrorCode;
    }

    return SystemError::noError;
}

//!Writes in synchronous mode
SystemError::ErrorCode NamedPipeSocket::write( const void* buf, unsigned int bytesToWrite, unsigned int* const bytesWritten )
{
    *bytesWritten = 0;
    while( *bytesWritten < bytesToWrite )
    {
        ssize_t numberOfBytesWritten = ::write( m_impl->hPipe, (const char*)buf + *bytesWritten, bytesToWrite - *bytesWritten );
        if( numberOfBytesWritten < 0 )
        {
            const int errCode = errno;
            if( errCode == EINTR )
                continue;
            return errCode;
        }
        *bytesWritten += numberOfBytesWritten;
    }

    return SystemError::noError;
}

//!Reads in synchronous mode
SystemError::ErrorCode NamedPipeSocket::read(
    void* buf,
    unsigned int bytesToRead,
    unsigned int* const bytesRead,
    int timeoutMs)
{
    timeval timeout{timeoutMs / 1000, timeoutMs % 1000};

    for (;;)
    {
        fd_set readFds;
        FD_ZERO(&readFds);
        FD_SET(m_impl->hPipe, &readFds);

        if (select(m_impl->hPipe + 1, &readFds, nullptr, nullptr, &timeout) == 1)
        {
            const ssize_t numberOfBytesRead = ::read(m_impl->hPipe, buf, bytesToRead);
            if (numberOfBytesRead < 0)
            {
                const int errCode = errno;
                if (errCode == EINTR)
                    continue;
                return errCode;
            }
            *bytesRead = numberOfBytesRead;
            return SystemError::noError;
        }
        else
        {
            return SystemError::timedOut;
        }
    }
}

SystemError::ErrorCode NamedPipeSocket::flush()
{
    //TODO/IMPL
    return SystemError::noError;
}

NamedPipeSocket::NamedPipeSocket( NamedPipeSocketImpl* implToUse )
:
    m_impl( implToUse )
{
}
