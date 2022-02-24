// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "dummy_socket.h"

namespace nx::network {

/**
 * Reads data from buffer. Sent data is ignored.
 */
class NX_NETWORK_API BufferSocket:
    public DummySocket
{
public:
    BufferSocket(const std::string& data);

    virtual bool close() override;
    virtual bool shutdown() override;
    virtual bool isClosed() const override;

    virtual bool connect(
        const SocketAddress& remoteSocketAddress,
        std::chrono::milliseconds timeout) override;

    virtual int recv(void* buffer, std::size_t bufferLen, int flags = 0) override;
    virtual int send(const void* buffer, std::size_t bufferLen) override;
    virtual bool isConnected() const override;

private:
    const std::string m_data;
    bool m_isOpened;
    size_t m_curPos;
};

} // namespace nx::network
