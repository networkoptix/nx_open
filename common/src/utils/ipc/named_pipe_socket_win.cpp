////////////////////////////////////////////////////////////
// 18 aug 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "named_pipe_socket.h"

#include "named_pipe_socket_win.h"


////////////////////////////////////////////////////////////
//// class NamedPipeSocketImpl
////////////////////////////////////////////////////////////
NamedPipeSocketImpl::NamedPipeSocketImpl()
:
    hPipe( INVALID_HANDLE_VALUE ),
    onServerSide( false )
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
    if( m_impl->onServerSide )
        DisconnectNamedPipe( m_impl->hPipe );
    else
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

    const QString win32PipeName = lit("\\\\.\\pipe\\%1").arg(pipeName);

    // TODO: use WaitNamedPipe

    m_impl->hPipe = CreateFile(
        reinterpret_cast<const wchar_t *>(win32PipeName.utf16()),             // pipe name 
        GENERIC_READ | GENERIC_WRITE,       // read/write access 
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL );

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
SystemError::ErrorCode NamedPipeSocket::read(
    void* buf,
    unsigned int bytesToRead,
    unsigned int* const bytesRead,
    int /*timeoutMs*/)
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
