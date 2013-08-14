////////////////////////////////////////////////////////////
// 18 aug 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifdef _WIN32

#include "named_pipe_socket.h"

#include "named_pipe_socket_win32.h"


////////////////////////////////////////////////////////////
//// class NamedPipeSocketImpl
////////////////////////////////////////////////////////////
NamedPipeSocketImpl::NamedPipeSocketImpl()
:
    hPipe( INVALID_HANDLE_VALUE )
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
    FlushFileBuffers( m_impl->hPipe );
    DisconnectNamedPipe( m_impl->hPipe );
    CloseHandle( m_impl->hPipe );
    m_impl->hPipe = INVALID_HANDLE_VALUE;

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
    //TODO/IMPL: #ak timeoutMillis support

    const QString win32PipeName = QString::fromLatin1("\\\\.\\pipe\\%1").arg(pipeName);
    m_impl->hPipe = CreateNamedPipe(
        win32PipeName.utf16(),             // pipe name 
        PIPE_ACCESS_DUPLEX,       // read/write access 
        PIPE_TYPE_MESSAGE |       // message type pipe 
            PIPE_READMODE_MESSAGE |   // message-read mode 
            PIPE_WAIT,                // blocking mode 
        PIPE_UNLIMITED_INSTANCES, // max. instances  
        NamedPipeSocketImpl::BUFSIZE,          // output buffer size 
        NamedPipeSocketImpl::BUFSIZE,          // input buffer size 
        0,                        // client time-out 
        NULL );                    // default security attribute 

    return m_impl->hPipe != INVALID_HANDLE_VALUE
        ? SystemError::noError
        : SystemError::getLastOSErrorCode();
}

//!Writes in synchronous mode
SystemError::ErrorCode NamedPipeSocket::write( const void* buf, unsigned int bytesToWrite, unsigned int* const bytesWritten )
{
    DWORD numberOfBytesWritten = 0;
    BOOL res = WriteFile(
        m_impl->hPipe,
        buf,
        bytesToWrite,
        &numberOfBytesWritten,
        NULL );
    *bytesWritten = numberOfBytesWritten;
    return !res || (numberOfBytesWritten != bytesToWrite)
        ? SystemError::getLastOSErrorCode()
        : SystemError::noError;
}

//!Reads in synchronous mode
SystemError::ErrorCode NamedPipeSocket::read( void* buf, unsigned int bytesToRead, unsigned int* const bytesRead )
{
    DWORD numberOfBytesRead = 0;
    BOOL res = ReadFile(
        m_impl->hPipe,
        buf,
        bytesToRead,
        &numberOfBytesRead,
        NULL );
    *bytesRead = numberOfBytesRead;

    return !res || (numberOfBytesRead == 0)
        ? SystemError::getLastOSErrorCode()
        : SystemError::noError;
}

SystemError::ErrorCode NamedPipeSocket::flush()
{
    return FlushFileBuffers( m_impl->hPipe );
}

NamedPipeSocket::NamedPipeSocket( NamedPipeSocketImpl* implToUse )
:
    m_impl( implToUse )
{
}

#endif
