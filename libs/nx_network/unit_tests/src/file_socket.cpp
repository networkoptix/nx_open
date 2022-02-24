// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "file_socket.h"

namespace nx {
namespace network {

FileSocket::FileSocket(const std::string& filePath):
    m_filePath(filePath),
    m_bytesRead(0)
{
}

bool FileSocket::close()
{
    m_file.close();
    return true;
}

bool FileSocket::isClosed() const
{
    return !m_file.is_open();
}

bool FileSocket::connect(
    const SocketAddress& remoteSocketAddress,
    std::chrono::milliseconds timeout)
{
    DummySocket::connect(remoteSocketAddress, timeout);

    if (m_file.is_open())
        m_file.close();
    m_file.open(m_filePath, std::ifstream::binary);
    return m_file.is_open();
}

int FileSocket::recv(void* buffer, std::size_t bufferLen, int /*flags*/)
{
    m_file.read((char*)buffer, bufferLen);
    m_bytesRead += m_file.gcount();
    return m_file.gcount();
}

int FileSocket::send(const void* /*buffer*/, std::size_t bufferLen)
{
    return bufferLen;
}

bool FileSocket::isConnected() const
{
    return m_file.is_open();
}

} // namespace network
} // namespace nx
