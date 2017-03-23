#include <thread>

#include <gtest/gtest.h>

#include <boost/optional.hpp>

#include <nx/network/async_channel_bridge.h>
#include <nx/utils/pipeline.h>
#include <nx/utils/random.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>

namespace nx {
namespace network {
namespace test {

class TestAsyncChannel:
    public aio::BasicPollable
{
    using base_type = aio::BasicPollable;

public:
    enum class InputDepletionPolicy
    {
        sendConnectionReset,
        ignore,
    };

    TestAsyncChannel(
        utils::pipeline::AbstractInput* input,
        utils::pipeline::AbstractOutput* output,
        InputDepletionPolicy inputDepletionPolicy)
        :
        m_input(input),
        m_output(output),
        m_inputDepletionPolicy(inputDepletionPolicy),
        m_errorState(false),
        m_totalBytesRead(0),
        m_readPaused(false),
        m_readBuffer(nullptr),
        m_sendPaused(false),
        m_sendBuffer(nullptr),
        m_readPosted(false),
        m_readSequence(0)
    {
        bindToAioThread(getAioThread());
    }

    virtual ~TestAsyncChannel() override
    {
        if (isInSelfAioThread())
            stopWhileInAioThread();
    }

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override
    {
        base_type::bindToAioThread(aioThread);

        m_reader.bindToAioThread(aioThread);
        m_writer.bindToAioThread(aioThread);
    }

    void readSomeAsync(
        nx::Buffer* const buffer,
        std::function<void(SystemError::ErrorCode, size_t)> handler)
    {
        NX_ASSERT(buffer->capacity() > buffer->size());
        
        QnMutexLocker lock(&m_mutex);
        m_readHandler = std::move(handler);
        m_readBuffer = buffer;
        if (m_readPaused)
            return;
        
        performAsyncRead(lock);
    }

    void sendAsync(
        const nx::Buffer& buffer,
        std::function<void(SystemError::ErrorCode, size_t)> handler)
    {
        QnMutexLocker lock(&m_mutex);

        m_sendHandler = std::move(handler);
        m_sendBuffer = &buffer;
        if (m_sendPaused)
            return;

        performAsyncSend(lock);
    }

    void cancelIOSync(nx::network::aio::EventType eventType)
    {
        if (eventType == nx::network::aio::EventType::etRead ||
            eventType == nx::network::aio::EventType::etNone)
        {
            m_reader.pleaseStopSync();
        }

        if (eventType == nx::network::aio::EventType::etWrite ||
            eventType == nx::network::aio::EventType::etNone)
        {
            m_writer.pleaseStopSync();
        }
    }

    void pauseReadingData()
    {
        QnMutexLocker lock(&m_mutex);
        m_readPaused = true;
    }

    void resumeReadingData()
    {
        QnMutexLocker lock(&m_mutex);
        m_readPaused = false;
        if (m_readHandler)
            performAsyncRead(lock);
    }

    void pauseSendingData()
    {
        QnMutexLocker lock(&m_mutex);
        m_sendPaused = true;
    }

    void resumeSendingData()
    {
        QnMutexLocker lock(&m_mutex);
        m_sendPaused = false;
        if (m_sendHandler)
            performAsyncSend(lock);
    }

    void waitForSomeDataToBeRead()
    {
        auto totalBytesReadBak = m_totalBytesRead.load();
        while (totalBytesReadBak == m_totalBytesRead)
            std::this_thread::yield();
    }

    void waitForReadSequenceToBreak()
    {
        auto readSequenceBak = m_readSequence.load();
        while (readSequenceBak == m_readSequence)
            std::this_thread::yield();
    }

    QByteArray dataRead() const
    {
        QnMutexLocker lock(&m_mutex);
        return m_totalDataRead;
    }

    void setErrorState()
    {
        m_errorState = true;
    }

private:
    InputDepletionPolicy m_inputDepletionPolicy;
    utils::pipeline::AbstractInput* m_input;
    utils::pipeline::AbstractOutput* m_output;
    std::atomic<bool> m_errorState;
    std::atomic<std::size_t> m_totalBytesRead;
    mutable QnMutex m_mutex;
    QByteArray m_totalDataRead;

    std::function<void(SystemError::ErrorCode, size_t)> m_readHandler;
    bool m_readPaused;
    nx::Buffer* m_readBuffer;

    std::function<void(SystemError::ErrorCode, size_t)> m_sendHandler;
    bool m_sendPaused;
    const nx::Buffer* m_sendBuffer;

    aio::BasicPollable m_reader;
    aio::BasicPollable m_writer;
    bool m_readPosted;
    std::atomic<int> m_readSequence;

    virtual void stopWhileInAioThread() override
    {
        m_reader.pleaseStopSync();
        m_writer.pleaseStopSync();
    }

    void handleInputDepletion(
        std::function<void(SystemError::ErrorCode, size_t)> handler)
    {
        switch (m_inputDepletionPolicy)
        {
            case InputDepletionPolicy::sendConnectionReset:
                handler(SystemError::connectionReset, (size_t)-1);
                break;
            case InputDepletionPolicy::ignore:
                break;
        }
    }

    void performAsyncRead(const QnMutexLockerBase& /*lock*/)
    {
        m_readPosted = true;
      
        m_reader.post(
            [this]()
            {
                decltype(m_readHandler) handler;
                handler.swap(m_readHandler);

                auto buffer = m_readBuffer;
                m_readBuffer = nullptr;

                m_readPosted = false;

                if (m_errorState)
                {
                    handler(SystemError::connectionReset, (size_t)-1);
                    return;
                }

                int bytesRead = m_input->read(
                    buffer->data() + buffer->size(),
                    buffer->capacity() - buffer->size());
                if (bytesRead > 0)
                {
                    {
                        QnMutexLocker lock(&m_mutex);
                        m_totalDataRead.append(buffer->data() + buffer->size(), bytesRead);
                        m_totalBytesRead += bytesRead;
                    }
                    buffer->resize(buffer->size() + bytesRead);

                    handler(SystemError::noError, bytesRead);

                    if (!m_readPosted)
                        ++m_readSequence;
                    return;
                }

                handleInputDepletion(std::move(handler));
            });
    }

    void performAsyncSend(const QnMutexLockerBase&)
    {
        decltype(m_sendHandler) handler;
        m_sendHandler.swap(handler);
        auto buffer = m_sendBuffer;
        m_sendBuffer = nullptr;

        m_writer.post(
            [this, buffer, handler = std::move(handler)]()
            {
                if (m_errorState)
                {
                    handler(SystemError::connectionReset, (size_t)-1);
                    return;
                }

                int bytesWritten = m_output->write(buffer->constData(), buffer->size());
                handler(
                    bytesWritten >= 0 ? SystemError::noError : SystemError::connectionReset,
                    bytesWritten);
            });
    }
};

class NotifyingOutput:
    public utils::pipeline::AbstractOutput
{
public:
    virtual int write(const void* data, size_t count) override
    {
        NX_ASSERT(count <= std::numeric_limits<int>::max());

        QnMutexLocker lock(&m_mutex);
        m_receivedData.append(static_cast<const char*>(data), (int)count);
        m_cond.wakeAll();
        return (int)count;
    }

    void waitForReceivedDataToMatch(const QByteArray& referenceData)
    {
        waitFor([this, &referenceData]() { return m_receivedData.startsWith(referenceData); });
    }

    void waitForSomeDataToBeReceived()
    {
        waitFor([this]() { return !m_receivedData.isEmpty(); });
    }

    const QByteArray& internalBuffer() const
    {
        return m_receivedData;
    }

private:
    QByteArray m_receivedData;
    QnMutex m_mutex;
    QnWaitCondition m_cond;

    template<typename ConditionFunc>
    void waitFor(ConditionFunc func)
    {
        QnMutexLocker lock(&m_mutex);
        while (!func())
            m_cond.wait(lock.mutex());
    }
};

//-------------------------------------------------------------------------------------------------
// Test fixture

class AsyncChannelBridge:
    public ::testing::Test
{
public:
    AsyncChannelBridge()
    {
        m_originalData.resize(nx::utils::random::number<int>(1, 1024*1024));
        std::generate(
            m_originalData.data(),
            m_originalData.data() + m_originalData.size(),
            rand);
    }

    ~AsyncChannelBridge()
    {
        if (m_bridge)
            m_bridge->pleaseStopSync();
    }

protected:
    void initializeFixedDataInput()
    {
        m_leftSource = std::make_unique<utils::pipeline::ReflectingPipeline>(m_originalData);
        m_rightSource = std::make_unique<utils::pipeline::ReflectingPipeline>(m_originalData);
    }

    void initializeInfiniteDataInput()
    {
        m_leftSource = std::make_unique<utils::pipeline::RandomDataSource>();
        m_rightSource = std::make_unique<utils::pipeline::RandomDataSource>();
    }
    
    void setInactivityTimeout()
    {
        m_inactivityTimeout = std::chrono::milliseconds(1);
        if (m_bridge)
            m_bridge->setInactivityTimeout(*m_inactivityTimeout);
    }

    void createChannel()
    {
        m_leftFile = std::make_unique<TestAsyncChannel>(
            m_leftSource.get(), &m_leftDest, TestAsyncChannel::InputDepletionPolicy::ignore);
        m_rightFile = std::make_unique<TestAsyncChannel>(
            m_rightSource.get(), &m_rightDest, TestAsyncChannel::InputDepletionPolicy::ignore);

        m_bridge = makeAsyncChannelBridge(m_leftFile.get(), m_rightFile.get());
        if (m_inactivityTimeout)
            m_bridge->setInactivityTimeout(*m_inactivityTimeout);
    }

    void exchangeData()
    {
        if (!m_bridge)
            createChannel();

        m_bridge->start(std::bind(&AsyncChannelBridge::onBridgeDone, this, std::placeholders::_1));
    }

    void startExchangingInfiniteData()
    {
        initializeInfiniteDataInput();
        createChannel();
        m_bridge->start(std::bind(&AsyncChannelBridge::onBridgeDone, this, std::placeholders::_1));
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
    
    void assertErrorHasBeenReported()
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

private:
    QByteArray m_originalData;
    std::unique_ptr<utils::pipeline::AbstractInput> m_leftSource;
    NotifyingOutput m_leftDest;
    std::unique_ptr<utils::pipeline::AbstractInput> m_rightSource;
    NotifyingOutput m_rightDest;
    std::unique_ptr<network::AsyncChannelBridge> m_bridge;
    std::unique_ptr<TestAsyncChannel> m_leftFile;
    std::unique_ptr<TestAsyncChannel> m_rightFile;
    nx::utils::promise<SystemError::ErrorCode> m_bridgeDone;
    boost::optional<std::chrono::milliseconds> m_inactivityTimeout;

    void onBridgeDone(SystemError::ErrorCode sysErrorCode)
    {
        m_bridgeDone.set_value(sysErrorCode);
    }
};

//-------------------------------------------------------------------------------------------------
// Test cases

TEST_F(AsyncChannelBridge, data_is_passed_through)
{
    initializeFixedDataInput();
    exchangeData();
    assertDataExchangeWasSuccessful();
}

TEST_F(AsyncChannelBridge, cancellation)
{
    startExchangingInfiniteData();
    waitForSomeExchangeToHappen();
    stopChannel();
}

TEST_F(AsyncChannelBridge, reports_error_from_source)
{
    startExchangingInfiniteData();
    waitForSomeExchangeToHappen();
    emulateErrorOnAnySource();
    assertErrorHasBeenReported();
}

TEST_F(AsyncChannelBridge, forwards_all_received_data_after_channel_closure)
{
    startExchangingInfiniteData();
    waitForSomeExchangeToHappen();
    pauseRightDestination();
    waitForSomeDataToBeReadFromLeftSource();
    closeLeftSource();
    resumeRightDestination();
    assertAllDataFromLeftHasBeenTransferredToTheRight();
}

TEST_F(AsyncChannelBridge, read_saturation)
{
    startExchangingInfiniteData();
    waitForSomeExchangeToHappen();

    pauseRightDestination();
    waitForChannelToStopReadingLeftSource();

    resumeRightDestination();
    waitForSomeExchangeToHappen();
}

TEST_F(AsyncChannelBridge, inactivity_timeout)
{
    setInactivityTimeout();
    startExchangingInfiniteData();
    pauseDataSources();
    assertBridgeDoneDueToInactivity();
}

//TEST_F(AsyncChannelBridge, channel_stops_sources_before_reporting_done)
//{
//    initializeFixedDataInput();
//    exchangeData();
//    assertDataExchangeWasSuccessful();
//    assertSourceHasBeenStoppedByChannel();
//}

//TEST_F(AsyncChannelBridge, deleting_object_within_done_handler)

//TEST_F(AsyncChannelBridge, takes_ownership_when_supplying_unique_ptr)

} // namespace test
} // namespace network
} // namespace nx
