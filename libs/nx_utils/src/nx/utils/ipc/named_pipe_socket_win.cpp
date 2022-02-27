// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "named_pipe_socket.h"

#include <QtCore/QString>

#include "named_pipe_socket_win.h"
#include "../string.h"

namespace nx::utils {

NamedPipeSocket::NamedPipeSocket():
    m_impl(new NamedPipeSocketImpl())
{
}

NamedPipeSocket::~NamedPipeSocket()
{
    FlushFileBuffers(m_impl->hPipe);
    if (m_impl->onServerSide)
        DisconnectNamedPipe(m_impl->hPipe);
    else
        CloseHandle(m_impl->hPipe);
    m_impl->hPipe = INVALID_HANDLE_VALUE;

    delete m_impl;
    m_impl = NULL;
}

SystemError::ErrorCode NamedPipeSocket::connectToServerSync(
    const std::string& pipeName,
    int /*timeoutMillis*/)
{
    // TODO: #akolesnikov timeoutMillis support

    // TODO: #akolesnikov Remove QString.
    const auto win32PipeName = QString::fromStdString(nx::utils::buildString("\\\\.\\pipe\\", pipeName));

    // TODO: use WaitNamedPipe
    m_impl->hPipe = CreateFile(
        reinterpret_cast<const wchar_t*>(win32PipeName.utf16()),             // pipe name
        GENERIC_READ | GENERIC_WRITE,       // read/write access
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    return m_impl->hPipe != INVALID_HANDLE_VALUE
        ? SystemError::noError
        : SystemError::getLastOSErrorCode();
}

//!Writes in synchronous mode
SystemError::ErrorCode NamedPipeSocket::write(
    const void* buf,
    unsigned int bytesToWrite,
    unsigned int* const bytesWritten)
{
    DWORD numberOfBytesWritten = 0;
    BOOL res = WriteFile(
        m_impl->hPipe,
        buf,
        bytesToWrite,
        &numberOfBytesWritten,
        NULL);
    *bytesWritten = numberOfBytesWritten;
    return !res || (numberOfBytesWritten != bytesToWrite)
        ? SystemError::getLastOSErrorCode()
        : SystemError::noError;
}

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
        NULL);
    *bytesRead = numberOfBytesRead;

    return !res || (numberOfBytesRead == 0)
        ? SystemError::getLastOSErrorCode()
        : SystemError::noError;
}

SystemError::ErrorCode NamedPipeSocket::flush()
{
    return FlushFileBuffers(m_impl->hPipe);
}

NamedPipeSocket::NamedPipeSocket(NamedPipeSocketImpl* implToUse):
    m_impl(implToUse)
{
}

} // namespace nx::utils
