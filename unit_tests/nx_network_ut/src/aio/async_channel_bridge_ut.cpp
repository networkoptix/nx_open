#include <thread>

#include <gtest/gtest.h>

#include <boost/optional.hpp>

#include <nx/network/aio/async_channel_bridge.h>
#include <nx/network/aio/abstract_async_channel.h>
#include <nx/utils/pipeline.h>
#include <nx/utils/random.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>

namespace nx {
namespace network {
namespace aio {
namespace test {

class TestAsyncChannel:
    public AbstractAsyncChannel
{
    using base_type = BasicPollable;

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

    virtual void bindToAioThread(AbstractAioThread* aioThread) override
    {
        base_type::bindToAioThread(aioThread);

        m_reader.bindToAioThread(aioThread);
        m_writer.bindToAioThread(aioThread);
    }

    virtual void readSomeAsync(
        nx::Buffer* const buffer,
        std::function<void(SystemError::ErrorCode, size_t)> handler) override
    {
        NX_ASSERT(buffer->capacity() > buffer->size());
        
        QnMutexLocker lock(&m_mutex);
        m_readHandler = std::move(handler);
        m_readBuffer = buffer;
        if (m_readPaused)
            return;
        
        performAsyncRead(lock);
    }

    virtual void sendAsync(
        const nx::Buffer& buffer,
        std::function<void(SystemError::ErrorCode, size_t)> handler) override
    {
        QnMutexLocker lock(&m_mutex);

        m_sendHandler = std::move(handler);
        m_sendBuffer = &buffer;
        if (m_sendPaused)
            return;

        performAsyncSend(lock);
    }

    virtual void cancelIOSync(EventType eventType) override
    {
        if (eventType == EventType::etRead ||
            eventType == EventType::etNone)
        {
            m_reader.pleaseStopSync();
        }

        if (eventType == EventType::etWrite ||
            eventType == EventType::etNone)
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

    BasicPollable m_reader;
    BasicPollable m_writer;
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
        createChannel();
        m_bridge->start(std::bind(&AioAsyncChannelBridge::onBridgeDone, this, std::placeholders::_1));
    }

    void startExchangingInfiniteData()
    {
        initializeInfiniteDataInput();
        createChannel();
        m_bridge->start(std::bind(&AioAsyncChannelBridge::onBridgeDone, this, std::placeholders::_1));
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

    void deleteBridge()
    {
        m_bridge.reset();
    }

private:
    QByteArray m_originalData;
    std::unique_ptr<utils::pipeline::AbstractInput> m_leftSource;
    NotifyingOutput m_leftDest;
    std::unique_ptr<utils::pipeline::AbstractInput> m_rightSource;
    NotifyingOutput m_rightDest;
    std::unique_ptr<aio::AsyncChannelBridge> m_bridge;
    std::unique_ptr<TestAsyncChannel> m_leftFile;
    std::unique_ptr<TestAsyncChannel> m_rightFile;
    nx::utils::promise<SystemError::ErrorCode> m_bridgeDone;
    boost::optional<std::chrono::milliseconds> m_inactivityTimeout;
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> m_additionalOnBridgeDoneHandler;

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
    assertErrorHasBeenReported();
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

class DummyAsyncChannel:
    public AbstractAsyncChannel
{
public:
    DummyAsyncChannel(int* dummyChannelCount):
        m_dummyChannelCount(dummyChannelCount)
    {
        ++(*m_dummyChannelCount);
    }

    virtual ~DummyAsyncChannel() override
    {
        --(*m_dummyChannelCount);
    }

    virtual void readSomeAsync(
        nx::Buffer* const /*buffer*/,
        std::function<void(SystemError::ErrorCode, size_t)> /*handler*/) override
    {
    }

    virtual void sendAsync(
        const nx::Buffer& /*buffer*/,
        std::function<void(SystemError::ErrorCode, size_t)> /*handler*/) override
    {
    }

    virtual void cancelIOSync(EventType /*eventType*/) override
    {
    }

private:
    int* m_dummyChannelCount;
};

TEST_F(AioAsyncChannelBridge, takes_ownership_when_supplying_unique_ptr)
{
    int dummyChannelCount = 0;

    auto left = std::make_unique<DummyAsyncChannel>(&dummyChannelCount);
    auto right = std::make_unique<DummyAsyncChannel>(&dummyChannelCount);
    ASSERT_EQ(2, dummyChannelCount);

    auto bridge = makeAsyncChannelBridge(std::move(left), std::move(right));
    bridge.reset();

    ASSERT_EQ(0, dummyChannelCount);
}

} // namespace test
} // namespace aio
} // namespace network
} // namespace nx
