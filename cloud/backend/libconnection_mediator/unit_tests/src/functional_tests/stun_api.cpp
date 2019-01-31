#include <gtest/gtest.h>

#include "mediator_functional_test.h"

namespace nx {
namespace hpm {
namespace test {

class StunApi:
    public MediatorFunctionalTest
{
public:
    ~StunApi()
    {
        if (m_connection)
            m_connection->pleaseStopSync();
    }

protected:
    virtual void SetUp() override
    {
        ASSERT_TRUE(startAndWaitUntilStarted());
    }

    void openInactiveConnection()
    {
        m_connection = std::make_unique<nx::network::TCPSocket>(AF_INET);
        ASSERT_TRUE(m_connection->connect(stunTcpEndpoint(), nx::network::kNoTimeout));
    }

    void waitForConnectionIsClosedByServer()
    {
        std::array<char, 256> readBuf;
        for (;;)
        {
            const int bytesRead = m_connection->recv(readBuf.data(), readBuf.size(), 0);
            if (bytesRead > 0)
                continue;
            if (bytesRead == 0 ||
                nx::network::socketCannotRecoverFromError(SystemError::getLastOSErrorCode()))
            {
                break;
            }
        }
    }

    void assertConnectionIsNotClosedDuring(std::chrono::milliseconds delay)
    {
        ASSERT_TRUE(m_connection->setRecvTimeout(delay.count()));

        std::array<char, 256> readBuf;
        ASSERT_EQ(-1, m_connection->recv(readBuf.data(), readBuf.size(), 0));
        ASSERT_TRUE(
            SystemError::getLastOSErrorCode() == SystemError::timedOut ||
            SystemError::getLastOSErrorCode() == SystemError::wouldBlock);
    }

private:
    std::unique_ptr<nx::network::TCPSocket> m_connection;
};

TEST_F(StunApi, inactive_connection_is_not_closed_without_inactivity_timeout)
{
    openInactiveConnection();
    assertConnectionIsNotClosedDuring(std::chrono::seconds(1));
}

//-------------------------------------------------------------------------------------------------

class StunApiLowInactivityTimeout:
    public StunApi
{
public:
    StunApiLowInactivityTimeout()
    {
        addArg("-stun/connectionInactivityTimeout", "100ms");
    }
};

TEST_F(StunApiLowInactivityTimeout, inactive_connection_is_closed_by_timeout)
{
    openInactiveConnection();
    waitForConnectionIsClosedByServer();
}

} // namespace test
} // namespace hpm
} // namespace nx
