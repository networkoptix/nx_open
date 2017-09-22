#include <thread>

#include <gtest/gtest.h>

#include <boost/optional.hpp>

#include <nx/network/aio/async_channel_bridge.h>
#include <nx/network/aio/abstract_async_channel.h>
#include <nx/network/aio/test/aio_test_async_channel.h>
#include <nx/utils/byte_stream/pipeline.h>
#include <nx/utils/random.h>
#include <nx/utils/test_support/test_pipeline.h>
#include <nx/utils/thread/wait_condition.h>

namespace nx {
namespace network {
namespace aio {
namespace test {

//-------------------------------------------------------------------------------------------------
// Test fixture

class AioAsyncChannelBridge:
    public ::testing::Test
{
public:
    AioAsyncChannelBridge()
    {
        m_originalData.resize(nx::utils::random::number<int>(1, 1024*1024));
        std::generate(
            m_originalData.data(),
            m_originalData.data() + m_originalData.size(),
            rand);
    }

    ~AioAsyncChannelBridge()
    {
        if (m_bridge)
            m_bridge->pleaseStopSync();
    }

protected:
    void setInactivityTimeout()
    {
        m_inactivityTimeout = std::chrono::milliseconds(1);
        if (m_bridge)
            m_bridge->setInactivityTimeout(*m_inactivityTimeout);
    }

    void setAdditionalOnBridgeDoneHandler(
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
    {
        m_additionalOnBridgeDoneHandler = std::move(handler);
    }

    void startExchangingFiniteData()
    {
        initializeFixedDataInput();
        createChannel(AsyncChannel::InputDepletionPolicy::ignore);
        m_bridge->start(std::bind(&AioAsyncChannelBridge::onBridgeDone, this, std::placeholders::_1));
    }

    void startExchangingInfiniteData()
    {
        initializeInfiniteDataInput();
        createChannel(AsyncChannel::InputDepletionPolicy::ignore);
        m_bridge->start(std::bind(&AioAsyncChannelBridge::onBridgeDone, this, std::placeholders::_1));
    }
    
    void givenIdleBridge()
    {
        initializeFixedDataInput();
        createChannel(AsyncChannel::InputDepletionPolicy::retry);
        m_bridge->start(std::bind(&AioAsyncChannelBridge::onBridgeDone, this, std::placeholders::_1));
        assertDataExchangeWasSuccessful();
    }

    void pauseRightDestination()
    {
        m_rightFile->pauseSendingData();
    }

    void resumeRightDestination()
    {
        m_rightFile->resumeSendingData();
    }

    void waitForSomeDataToBeReadFromLeftSource()
    {
        m_leftFile->waitForSomeDataToBeRead();
    }

    void waitForChannelToStopReadingLeftSource()
    {
        m_leftFile->waitForReadSequenceToBreak();
    }

    void waitForSomeExchangeToHappen()
    {
        m_rightDest.waitForSomeDataToBeReceived();
        m_leftDest.waitForSomeDataToBeReceived();
    }

    void pauseDataSources()
    {
        m_leftFile->pauseReadingData();
        m_rightFile->pauseReadingData();
    }

    void stopChannel()
    {
        m_bridge->pleaseStopSync();
    }

    void closeLeftSource()
    {
        m_leftFile->setErrorState();
    }

    void emulateErrorOnAnySource()
    {
        m_leftFile->setErrorState();
    }
    
    void assertBridgeIsDoneDueToError()
    {
        ASSERT_NE(SystemError::noError, m_bridgeDone.get_future().get());
    }

    void assertDataExchangeWasSuccessful()
    {
        m_rightDest.waitForReceivedDataToMatch(m_originalData);
        m_leftDest.waitForReceivedDataToMatch(m_originalData);
    }

    void assertAllDataFromLeftHasBeenTransferredToTheRight()
    {
        m_rightDest.waitForReceivedDataToMatch(m_leftFile->dataRead());
    }

    void assertBridgeDoneDueToInactivity()
    {
        ASSERT_EQ(SystemError::timedOut, m_bridgeDone.get_future().get());
    }

    void deleteBridge()
    {
        m_bridge.reset();
    }

private:
    QByteArray m_originalData;
    std::unique_ptr<utils::bstream::AbstractInput> m_leftSource;
    utils::bstream::test::NotifyingOutput m_leftDest;
    std::unique_ptr<utils::bstream::AbstractInput> m_rightSource;
    utils::bstream::test::NotifyingOutput m_rightDest;
    std::unique_ptr<aio::AsyncChannelBridge> m_bridge;
    std::unique_ptr<AsyncChannel> m_leftFile;
    std::unique_ptr<AsyncChannel> m_rightFile;
    nx::utils::promise<SystemError::ErrorCode> m_bridgeDone;
    boost::optional<std::chrono::milliseconds> m_inactivityTimeout;
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> m_additionalOnBridgeDoneHandler;

    void createChannel(AsyncChannel::InputDepletionPolicy inputDepletionPolicy)
    {
        m_leftFile = std::make_unique<AsyncChannel>(
            m_leftSource.get(), &m_leftDest, inputDepletionPolicy);
        m_rightFile = std::make_unique<AsyncChannel>(
            m_rightSource.get(), &m_rightDest, inputDepletionPolicy);

        m_bridge = makeAsyncChannelBridge(m_leftFile.get(), m_rightFile.get());
        if (m_inactivityTimeout)
            m_bridge->setInactivityTimeout(*m_inactivityTimeout);
    }

    void initializeFixedDataInput()
    {
        m_leftSource = std::make_unique<utils::bstream::Pipe>(m_originalData);
        m_rightSource = std::make_unique<utils::bstream::Pipe>(m_originalData);
    }

    void initializeInfiniteDataInput()
    {
        m_leftSource = std::make_unique<utils::bstream::RandomDataSource>();
        m_rightSource = std::make_unique<utils::bstream::RandomDataSource>();
    }

    void onBridgeDone(SystemError::ErrorCode sysErrorCode)
    {
        if (m_additionalOnBridgeDoneHandler)
            m_additionalOnBridgeDoneHandler(sysErrorCode);
        m_bridgeDone.set_value(sysErrorCode);
    }
};

//-------------------------------------------------------------------------------------------------
// Test cases

TEST_F(AioAsyncChannelBridge, data_is_passed_through)
{
    startExchangingFiniteData();
    assertDataExchangeWasSuccessful();
}

TEST_F(AioAsyncChannelBridge, cancellation)
{
    startExchangingInfiniteData();
    waitForSomeExchangeToHappen();
    stopChannel();
}

TEST_F(AioAsyncChannelBridge, reports_error_from_source)
{
    startExchangingInfiniteData();
    waitForSomeExchangeToHappen();
    emulateErrorOnAnySource();
    assertBridgeIsDoneDueToError();
}

TEST_F(AioAsyncChannelBridge, forwards_all_received_data_after_channel_closure)
{
    startExchangingInfiniteData();
    waitForSomeExchangeToHappen();
    pauseRightDestination();
    waitForSomeDataToBeReadFromLeftSource();
    closeLeftSource();
    resumeRightDestination();
    assertAllDataFromLeftHasBeenTransferredToTheRight();
}

TEST_F(AioAsyncChannelBridge, bridge_is_done_after_either_channel_is_closed)
{
    givenIdleBridge();
    closeLeftSource();
    assertBridgeIsDoneDueToError();
}

TEST_F(AioAsyncChannelBridge, read_saturation)
{
    startExchangingInfiniteData();
    waitForSomeExchangeToHappen();

    pauseRightDestination();
    waitForChannelToStopReadingLeftSource();

    resumeRightDestination();
    waitForSomeExchangeToHappen();
}

TEST_F(AioAsyncChannelBridge, inactivity_timeout)
{
    setInactivityTimeout();
    startExchangingInfiniteData();
    pauseDataSources();
    assertBridgeDoneDueToInactivity();
}

TEST_F(AioAsyncChannelBridge, deleting_object_within_done_handler)
{
    setAdditionalOnBridgeDoneHandler(
        [this](SystemError::ErrorCode) { deleteBridge(); });
    startExchangingFiniteData();
    assertDataExchangeWasSuccessful();
}

//-------------------------------------------------------------------------------------------------

class DummyAsyncChannel:
    public AbstractAsyncChannel
{
public:
    DummyAsyncChannel(
        int* dummyChannelCount,
        int* pleaseStopCallCount)
        :
        m_dummyChannelCount(dummyChannelCount),
        m_pleaseStopCallCount(pleaseStopCallCount)
    {
        ++(*m_dummyChannelCount);
    }

    virtual ~DummyAsyncChannel() override
    {
        --(*m_dummyChannelCount);
    }

    virtual void pleaseStopSync(bool /*x*/ = false) override
    {
        ++(*m_pleaseStopCallCount);
    }

    virtual void readSomeAsync(
        nx::Buffer* const /*buffer*/,
        IoCompletionHandler /*handler*/) override
    {
    }

    virtual void sendAsync(
        const nx::Buffer& /*buffer*/,
        IoCompletionHandler /*handler*/) override
    {
    }

    virtual void cancelIOSync(EventType /*eventType*/) override
    {
    }

private:
    int* m_dummyChannelCount;
    int* m_pleaseStopCallCount;
};

TEST_F(AioAsyncChannelBridge, takes_ownership_when_supplying_unique_ptr)
{
    int dummyChannelCount = 0;
    int pleaseStopCallCount = 0;

    auto left = std::make_unique<DummyAsyncChannel>(&dummyChannelCount, &pleaseStopCallCount);
    auto right = std::make_unique<DummyAsyncChannel>(&dummyChannelCount, &pleaseStopCallCount);
    ASSERT_EQ(2, dummyChannelCount);

    auto bridge = makeAsyncChannelBridge(std::move(left), std::move(right));
    bridge->pleaseStopSync();
    bridge.reset();

    ASSERT_EQ(2, pleaseStopCallCount);
    ASSERT_EQ(0, dummyChannelCount);
}

TEST_F(AioAsyncChannelBridge, channels_created_on_stack)
{
    int dummyChannelCount = 0;
    int pleaseStopCallCount = 0;

    DummyAsyncChannel left(&dummyChannelCount, &pleaseStopCallCount);
    DummyAsyncChannel right(&dummyChannelCount, &pleaseStopCallCount);
    ASSERT_EQ(2, dummyChannelCount);

    auto bridge = makeAsyncChannelBridge(&left, &right);
    bridge->pleaseStopSync();
    bridge.reset();

    ASSERT_EQ(2, dummyChannelCount);
    ASSERT_EQ(2, pleaseStopCallCount);
}

} // namespace test
} // namespace aio
} // namespace network
} // namespace nx
