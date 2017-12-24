#include <gtest/gtest.h>

#include <QtCore/QElapsedTimer>

#include <nx/network/time/time_protocol_server.h>
#include <nx/utils/random.h>
#include <nx/utils/sync_call.h>
#include <nx/utils/time.h>

namespace nx {
namespace network {
namespace test {

template<typename TimeProtocolClientType>
class BasicTimeProtocolClient:
    public ::testing::Test
{
public:
    BasicTimeProtocolClient():
        m_timeShift(utils::test::ClockType::system)
    {
    }

protected:
    void givenAlreadyUsedClient()
    {
        whenRequestedTime();
        thenCorrectTimeHasBeenReported();
    }

    void whenTimeFetchFailedDueToServerError()
    {
        m_timeServer->pleaseStopSync();
        m_timeServer.reset();

        whenRequestedTime();
        thenErrorHasBeenReported();
    }

    void thenTimeFetchWorksAgainAfterServerIsAvailable()
    {
        initializeServer();

        whenRequestedTime();
        thenCorrectTimeHasBeenReported();
    }

    void whenRequestedTime()
    {
        using namespace std::placeholders;

        m_timeRequestResult = makeSyncCall<qint64, SystemError::ErrorCode>(
            std::bind(&TimeProtocolClientType::getTimeAsync,
                m_timeProtocolClient.get(), _1));
    }

    void whenShiftingCurrentTime()
    {
        m_timeShift.applyAbsoluteShift(
            std::chrono::seconds(
                nx::utils::random::number<int>(-10000, 10000)));
    }

    void thenCorrectTimeHasBeenReported()
    {
        using namespace std::chrono;

        ASSERT_EQ(SystemError::noError, std::get<1>(m_timeRequestResult));
        ASSERT_LE(
            std::get<0>(m_timeRequestResult),
            duration_cast<milliseconds>(nx::utils::timeSinceEpoch()).count());
    }

    void thenErrorHasBeenReported()
    {
        ASSERT_NE(SystemError::noError, std::get<1>(m_timeRequestResult));
    }

private:
    std::unique_ptr<TimeProtocolServer> m_timeServer;
    std::unique_ptr<TimeProtocolClientType> m_timeProtocolClient;
    std::tuple<qint64, SystemError::ErrorCode> m_timeRequestResult;
    utils::test::ScopedTimeShift m_timeShift;
    boost::optional<SocketAddress> m_serverAddress;

    virtual void SetUp() override
    {
        initializeServer();

        m_timeProtocolClient = std::make_unique<TimeProtocolClientType>(
            m_timeServer->address());
    }

    virtual void TearDown() override
    {
        m_timeProtocolClient->pleaseStopSync();
        m_timeServer->pleaseStopSync();
    }

    void initializeServer()
    {
        m_timeServer = std::make_unique<TimeProtocolServer>(false);

        ASSERT_TRUE(m_timeServer->bind(
            m_serverAddress ? (*m_serverAddress) : SocketAddress::anyPrivateAddress));
        ASSERT_TRUE(m_timeServer->listen());

        m_serverAddress = m_timeServer->address();
    }
};

TYPED_TEST_CASE_P(BasicTimeProtocolClient);

//-------------------------------------------------------------------------------------------------
// Test cases.

TYPED_TEST_P(BasicTimeProtocolClient, fetches_time)
{
    this->whenRequestedTime();
    this->thenCorrectTimeHasBeenReported();
}

TYPED_TEST_P(BasicTimeProtocolClient, reusing_same_instance)
{
    for (int i = 0; i < 3; ++i)
    {
        this->whenShiftingCurrentTime();
        this->whenRequestedTime();
        this->thenCorrectTimeHasBeenReported();
    }
}

TYPED_TEST_P(BasicTimeProtocolClient, reusing_same_instance_after_error)
{
    this->givenAlreadyUsedClient();

    this->whenShiftingCurrentTime();
    this->whenTimeFetchFailedDueToServerError();

    this->thenTimeFetchWorksAgainAfterServerIsAvailable();
}

REGISTER_TYPED_TEST_CASE_P(BasicTimeProtocolClient,
    fetches_time,
    reusing_same_instance,
    reusing_same_instance_after_error);

} // namespace test
} // namespace nx
} // namespace network
