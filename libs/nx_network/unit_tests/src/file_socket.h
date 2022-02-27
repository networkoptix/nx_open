// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <fstream>

#include <nx/network/test_support/dummy_socket.h>

namespace nx {
namespace network {

/**
 * Reads data from file. Sent data is just ignored.
 */
class FileSocket:
    public DummySocket
{
public:
    FileSocket( const std::string& filePath );

    virtual bool close() override;
    virtual bool isClosed() const override;

    virtual bool connect(
        const SocketAddress& remoteSocketAddress,
        std::chrono::milliseconds timeout) override;
    virtual int recv( void* buffer, std::size_t bufferLen, int flags = 0 ) override;
    virtual int send( const void* buffer, std::size_t bufferLen ) override;
    virtual bool isConnected() const override;

private:
    std::string m_filePath;
    std::ifstream m_file;
    size_t m_bytesRead;
};

} // namespace network
} // namespace nx
