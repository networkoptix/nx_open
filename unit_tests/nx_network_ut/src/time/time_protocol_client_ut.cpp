#include <gtest/gtest.h>

#include <QtCore/QElapsedTimer>

#include <nx/network/time/time_protocol_client.h>
#include <nx/network/time/time_protocol_server.h>
#include <nx/utils/sync_call.h>

namespace nx {
namespace network {
namespace test {

class TimeProtocolClient:
    public ::testing::Test
{
public:
    TimeProtocolClient():
        m_timeServer(false)
    {
    }

protected:
    void whenRequestedTime()
    {
        using namespace std::placeholders;

        m_timeRequestResult = makeSyncCall<qint64, SystemError::ErrorCode>(
            std::bind(&network::TimeProtocolClient::getTimeAsync,
                m_timeProtocolClient.get(), _1));
    }
    
    void thenTimeHasBeenReported()
    {
        ASSERT_EQ(
            SystemError::noError,
            std::get<SystemError::ErrorCode>(m_timeRequestResult));
        ASSERT_GE(std::get<qint64>(m_timeRequestResult), ::time(NULL));
    }

private:
    TimeProtocolServer m_timeServer;
    std::unique_ptr<network::TimeProtocolClient> m_timeProtocolClient;
    std::tuple<qint64, SystemError::ErrorCode> m_timeRequestResult;

    virtual void SetUp() override
    {
        ASSERT_TRUE(m_timeServer.bind(SocketAddress::anyPrivateAddress));
        ASSERT_TRUE(m_timeServer.listen());

        m_timeProtocolClient = std::make_unique<network::TimeProtocolClient>(
            m_timeServer.address());
    }

    virtual void TearDown() override
    {
        m_timeProtocolClient->pleaseStopSync();
        m_timeServer.pleaseStopSync();
    }
};

TEST_F(TimeProtocolClient, fetches_time)
{
    whenRequestedTime();
    thenTimeHasBeenReported();
}

TEST_F(TimeProtocolClient, reusing_same_instance)
{
    for (int i = 0; i < 3; ++i)
    {
        whenRequestedTime();
        thenTimeHasBeenReported();
    }
}

} // namespace test
} // namespace nx
} // namespace network
