#include <memory>

#include <nx/network/system_socket.h>

#include <nx/cloud/vms_gateway/vms_gateway_process.h>

#include "../test_setup.h"

namespace nx {
namespace cloud {
namespace gateway {
namespace test {

class HttpServer:
    public BasicComponentTest
{
public:
    HttpServer()
    {
        addArg("--http/connectionInactivityTimeout=1ms");
    }

protected:
    virtual void SetUp() override
    {
        ASSERT_TRUE(startAndWaitUntilStarted());

        setHttpEndpoint(moduleInstance()->impl()->httpEndpoints().front());
    }

    void setHttpEndpoint(const nx::network::SocketAddress& httpEndpoint)
    {
        m_httpEndpoint = httpEndpoint;
    }

    void establishSilentConnection()
    {
        m_clientConnection = std::make_unique<nx::network::TCPSocket>(AF_INET);
        ASSERT_TRUE(m_clientConnection->connect(m_httpEndpoint, nx::network::kNoTimeout))
            << SystemError::getLastOSErrorText().toStdString();
    }

    void waitForConnectionIsDroppedByServer()
    {
        std::array<char, 128> readBuf;

        for (;;)
        {
            int bytesRead = m_clientConnection->recv(readBuf.data(), readBuf.size(), 0);
            if (bytesRead == 0)
                break;
            if (bytesRead > 0)
                continue;

            if (nx::network::socketCannotRecoverFromError(SystemError::getLastOSErrorCode()))
                break;
        }
    }

private:
    nx::network::SocketAddress m_httpEndpoint;
    std::unique_ptr<nx::network::TCPSocket> m_clientConnection;
};

TEST_F(HttpServer, inactive_not_identified_connection_is_dropped_by_timeout)
{
    establishSilentConnection();
    waitForConnectionIsDroppedByServer();
}

} // namespace test
} // namespace gateway
} // namespace cloud
} // namespace nx
