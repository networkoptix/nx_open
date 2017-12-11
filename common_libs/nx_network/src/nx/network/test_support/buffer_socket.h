#pragma once

#include "dummy_socket.h"

namespace nx {
namespace network {

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
        unsigned int timeoutMillis = kDefaultTimeoutMillis) override;

    virtual int recv(void* buffer, unsigned int bufferLen, int flags = 0) override;
    virtual int send(const void* buffer, unsigned int bufferLen) override;
    virtual bool isConnected() const override;

private:
    const std::string& m_data;
    bool m_isOpened;
    size_t m_curPos;
};

} // namespace network
} // namespace nx
