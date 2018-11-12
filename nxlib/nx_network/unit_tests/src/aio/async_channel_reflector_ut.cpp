#include <gtest/gtest.h>

#include <nx/network/aio/async_channel_reflector.h>
#include <nx/network/aio/test/aio_test_async_channel.h>
#include <nx/utils/random.h>
#include <nx/utils/std/future.h>
#include <nx/utils/test_support/test_pipeline.h>

namespace nx {
namespace network {
namespace aio {
namespace test {

class AsyncChannelReflector:
    public ::testing::Test
{
public:
    AsyncChannelReflector():
        m_channelToReflect(nullptr)
    {
        using namespace std::placeholders;

        auto channelToReflect = std::make_unique<AsyncChannel>(
            &m_input,
            &m_output,
            AsyncChannel::InputDepletionPolicy::retry);
        m_channelToReflect = channelToReflect.get();
        auto reflector = makeAsyncChannelReflector(std::move(channelToReflect));
        reflector->start(std::bind(&AsyncChannelReflector::onReflectorDone, this, _1));
        m_reflector = std::move(reflector);
    }

    ~AsyncChannelReflector()
    {
        if (m_reflector)
            m_reflector->pleaseStopSync();
    }

protected:
    void sendThroughSomeData()
    {
        whenSentSomeData();
        thenSameDataHasBeenReflected();
    }

    void emulateSendTimeoutOnUnderlyingChannel()
    {
        m_channelToReflect->setSendErrorState(SystemError::timedOut);
    }

    void emulateRecvTimeoutOnUnderlyingChannel()
    {
        m_channelToReflect->setReadErrorState(SystemError::timedOut);
    }

    void whenSentSomeData()
    {
        prepareTestData();
        m_input.write(m_testData.constData(), m_testData.size());
    }

    void whenEmulateIoErrorOnInput()
    {
        m_channelToReflect->setErrorState();
    }

    void whenUnderlyingChannelIsFullyFunctionalAgain()
    {
        m_channelToReflect->setSendErrorState(boost::none);
        m_channelToReflect->setReadErrorState(boost::none);
    }

    void thenSameDataHasBeenReflected()
    {
        m_output.waitForReceivedDataToMatch(m_testData);
    }

    void thenReflectorReportsError()
    {
        ASSERT_NE(SystemError::noError, m_reflectorDone.get_future().get());
    }

    void thenSendTimedoutHasBeenRaisedOnUnderlyingChannel()
    {
        m_channelToReflect->waitForAnotherSendErrorReported();
    }

    void thenRecvTimedoutHasBeenRaisedOnUnderlyingChannel()
    {
        m_channelToReflect->waitForAnotherReadErrorReported();
    }

private:
    std::unique_ptr<BasicPollable> m_reflector;
    utils::bstream::Pipe m_input;
    utils::bstream::test::NotifyingOutput m_output;
    nx::Buffer m_testData;
    utils::promise<SystemError::ErrorCode> m_reflectorDone;
    AsyncChannel* m_channelToReflect;

    void prepareTestData()
    {
        m_testData.resize(nx::utils::random::number<int>(1024, 4096));
        std::generate(m_testData.data(), m_testData.data() + m_testData.size(), &rand);
    }

    void onReflectorDone(SystemError::ErrorCode sysErrorCode)
    {
        m_reflectorDone.set_value(sysErrorCode);
    }
};

TEST_F(AsyncChannelReflector, actually_reflects_data)
{
    whenSentSomeData();
    thenSameDataHasBeenReflected();
}

TEST_F(AsyncChannelReflector, reports_underlying_channel_io_error)
{
    sendThroughSomeData();
    whenEmulateIoErrorOnInput();
    thenReflectorReportsError();
}

TEST_F(AsyncChannelReflector, properly_handles_send_timedout)
{
    emulateSendTimeoutOnUnderlyingChannel();

    whenSentSomeData();
    thenSendTimedoutHasBeenRaisedOnUnderlyingChannel();

    whenUnderlyingChannelIsFullyFunctionalAgain();
    thenSameDataHasBeenReflected();
}

TEST_F(AsyncChannelReflector, properly_handles_recv_timedout)
{
    emulateRecvTimeoutOnUnderlyingChannel();

    whenSentSomeData();
    thenRecvTimedoutHasBeenRaisedOnUnderlyingChannel();

    whenUnderlyingChannelIsFullyFunctionalAgain();
    thenSameDataHasBeenReflected();
}

} // namespace test
} // namespace aio
} // namespace network
} // namespace nx
