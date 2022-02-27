// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <memory>

#include <gtest/gtest.h>

#include <nx/network/aio/stream_transforming_async_channel.h>
#include <nx/network/aio/test/aio_test_async_channel.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx {
namespace network {
namespace aio {
namespace test {

class Converter:
    public utils::bstream::Converter
{
public:
    int write(const void* data, size_t count)
    {
        const int result = m_outputStream->write(data, count);
        if (m_writeResultToReportUnconditionally)
            return *m_writeResultToReportUnconditionally;
        return result;
    }

    int read(void* data, size_t count)
    {
        return m_inputStream->read(data, count);
    }

    bool eof() const
    {
        return false;
    }

    bool failed() const
    {
        return false;
    }

    void setWriteResultToReportUnconditionally(
        std::optional<int> resultCode)
    {
        m_writeResultToReportUnconditionally = resultCode;
    }

private:
    std::optional<int> m_writeResultToReportUnconditionally;
};

class ChannelWriter
{
public:
    ChannelWriter(aio::AbstractAsyncChannel* channel):
        m_channel(channel)
    {
    }

    SystemError::ErrorCode writeSync(nx::Buffer data)
    {
        nx::utils::promise<SystemError::ErrorCode> written;
        m_channel->sendAsync(
            &data,
            [&written](SystemError::ErrorCode sysErrorCode, std::size_t /*bytesWritten*/)
            {
                written.set_value(sysErrorCode);
            });

        return written.get_future().get();
    }

private:
    aio::AbstractAsyncChannel* m_channel;
};

class ChannelReader
{
public:
    ChannelReader(aio::AbstractAsyncChannel* channel):
        m_channel(channel),
        m_totalBytesRead(0)
    {
    }

    void start()
    {
        scheduleNextRead();
    }

    void waitForSomeData()
    {
        while (m_totalBytesRead == 0)
            std::this_thread::yield();
    }

    void waitEof()
    {
        m_readError.get_future().wait();
    }

    nx::Buffer dataRead() const
    {
        return m_dataRead;
    }

private:
    aio::AbstractAsyncChannel* m_channel;
    nx::Buffer m_dataRead;
    nx::Buffer m_readBuffer;
    nx::utils::promise<SystemError::ErrorCode> m_readError;
    std::atomic<std::size_t> m_totalBytesRead;

    void scheduleNextRead()
    {
        using namespace std::placeholders;

        constexpr static int kReadBufSize = 16*1024;

        m_readBuffer.clear();
        m_readBuffer.reserve(kReadBufSize);
        m_channel->readSomeAsync(
            &m_readBuffer,
            std::bind(&ChannelReader::onBytesRead, this, _1, _2));
    }

    void onBytesRead(SystemError::ErrorCode sysErrorCode, std::size_t bytesRead)
    {
        if (sysErrorCode == SystemError::noError && bytesRead > 0)
        {
            m_dataRead.append(m_readBuffer);
            m_totalBytesRead += bytesRead;
            scheduleNextRead();
            return;
        }

        m_readError.set_value(sysErrorCode);
    }
};

//-------------------------------------------------------------------------------------------------
// Test fixture.

class StreamTransformingAsyncChannel:
    public ::testing::Test
{
public:
    StreamTransformingAsyncChannel():
        m_rawDataChannel(nullptr),
        m_readCallSequence(0)
    {
    }

    ~StreamTransformingAsyncChannel()
    {
        m_channel->pleaseStopSync();
    }

protected:
    void givenReflectingRawChannel()
    {
        createTransformingChannel();
    }

    void givenScheduledRead()
    {
        using namespace std::placeholders;

        if (!m_channel)
            createTransformingChannel();

        m_channel->readSomeAsync(
            &m_readBuffer,
            std::bind(&StreamTransformingAsyncChannel::onSomeBytesRead, this,
                m_readCallSequence, _1, _2));
    }

    void givenScheduledWrite()
    {
        using namespace std::placeholders;

        if (!m_channel)
            createTransformingChannel();

        m_converter->setWriteResultToReportUnconditionally(
            utils::bstream::StreamIoError::wouldBlock);
        m_rawDataChannel->pauseSendingData();

        prepareRandomInputData();
        m_channel->sendAsync(
            &m_inputData,
            std::bind(&StreamTransformingAsyncChannel::onBytesWritten, this, _1, _2));
    }

    void whenSendSomeData()
    {
        using namespace std::placeholders;

        if (!m_channel)
            createTransformingChannel();
        prepareRandomInputData();
        m_channel->sendAsync(
            &m_inputData,
            std::bind(&StreamTransformingAsyncChannel::onBytesWritten, this, _1, _2));
    }

    void whenCancelledRead()
    {
        m_channel->cancelIOSync(aio::EventType::etRead);
    }

    void whenCancelledWrite()
    {
        m_channel->cancelIOSync(aio::EventType::etWrite);
    }

    void whenTransferredFiniteData()
    {
        prepareRandomInputData();

        sendTestData();
        thenSendHasSucceeded();

        m_reflectingPipeline.writeEof();
    }

    void whenSendFiniteData()
    {
        prepareRandomInputData();
        sendTestData();
    }

    void thenSendHasSucceeded()
    {
        ASSERT_EQ(SystemError::noError, m_sendResultQueue.pop());
    }

    void thenSendHasFailed()
    {
        ASSERT_NE(SystemError::noError, m_sendResultQueue.pop());
    }

    void assertRawChannelHasCompletedSend()
    {
        ASSERT_FALSE(m_rawDataChannel->isWriteScheduled());
    }

    void assertDataReadMatchesDataWritten()
    {
        readTestData();
        ASSERT_EQ(m_expectedOutputData, m_outputData);
    }

    void assertIoHasBeenCancelledOnRawDataChannel()
    {
        assertReadHasBeenCancelledOnRawDataChannel();
        assertWriteHasBeenCancelledOnRawDataChannel();
    }

    void assertReadHasBeenCancelledOnRawDataChannel()
    {
        ASSERT_FALSE(m_rawDataChannel->isReadScheduled());
    }

    void assertWriteHasBeenCancelledOnRawDataChannel()
    {
        ASSERT_FALSE(m_rawDataChannel->isWriteScheduled());
    }

    void assertIoHasBeenProperlyCancelled()
    {
        assertIoHasBeenCancelledOnRawDataChannel();

        // Issuing read operation that produces some data. Ensuring handler has been called.
        ++m_readCallSequence;

        m_rawDataChannel->resumeSendingData();
        m_converter->setWriteResultToReportUnconditionally(std::nullopt);

        whenTransferredFiniteData();
        assertDataReadMatchesDataWritten();
    }

    AsyncChannel* rawDataChannel()
    {
        return m_rawDataChannel;
    }

    aio::StreamTransformingAsyncChannel& channel()
    {
        if (!m_channel)
            createTransformingChannel();

        return *m_channel;
    }

private:
    std::unique_ptr<aio::StreamTransformingAsyncChannel> m_channel;
    AsyncChannel* m_rawDataChannel;
    std::unique_ptr<Converter> m_converter;
    utils::bstream::Pipe m_reflectingPipeline;
    nx::Buffer m_inputData;
    nx::Buffer m_expectedOutputData;
    nx::Buffer m_outputData;
    nx::Buffer m_readBuffer;
    int m_readCallSequence;
    nx::utils::SyncQueue<SystemError::ErrorCode> m_sendResultQueue;

    void createTransformingChannel()
    {
        auto rawDataChannel = std::make_unique<AsyncChannel>(
            &m_reflectingPipeline,
            &m_reflectingPipeline,
            AsyncChannel::InputDepletionPolicy::retry);
        m_rawDataChannel = rawDataChannel.get();

        m_converter = std::make_unique<Converter>();
        m_channel = std::make_unique<aio::StreamTransformingAsyncChannel>(
            std::move(rawDataChannel),
            m_converter.get());
    }

    void sendTestData()
    {
        ChannelWriter writer(m_channel.get());
        m_sendResultQueue.push(writer.writeSync(m_inputData));
    }

    void readTestData()
    {
        ChannelReader reader(m_channel.get());
        reader.start();
        reader.waitForSomeData();
        reader.waitEof();
        m_outputData = reader.dataRead();
    }

    void onSomeBytesRead(
        int expectedReadCallSequence,
        SystemError::ErrorCode /*sysErrorCode*/,
        std::size_t /*bytesRead*/)
    {
        ASSERT_EQ(m_readCallSequence, expectedReadCallSequence);
    }

    void onBytesWritten(
        SystemError::ErrorCode sysErrorCode,
        std::size_t /*bytesWritten*/)
    {
        m_sendResultQueue.push(sysErrorCode);
    }

    void waitUntilReadHasBeenScheduledOnRawDataChannel()
    {
        while (!m_rawDataChannel->isReadScheduled())
            std::this_thread::yield();
    }

    void waitUntilWriteHasBeenScheduledOnRawDataChannel()
    {
        while (!m_rawDataChannel->isWriteScheduled())
            std::this_thread::yield();
    }

    void prepareRandomInputData()
    {
        m_inputData.resize(1024);
        std::generate(m_inputData.begin(), m_inputData.end(), rand);
        m_expectedOutputData = m_inputData;
    }
};

//-------------------------------------------------------------------------------------------------
// Test cases.

TEST_F(StreamTransformingAsyncChannel, read_write)
{
    givenReflectingRawChannel();
    whenTransferredFiniteData();
    assertDataReadMatchesDataWritten();
}

//TEST_F(StreamTransformingAsyncChannel, read_originates_read_and_write_on_raw_channel)

TEST_F(StreamTransformingAsyncChannel, cancel_read_scheduled_from_non_aio_thread)
{
    givenScheduledRead();
    whenCancelledRead();
    assertIoHasBeenProperlyCancelled();
}

TEST_F(StreamTransformingAsyncChannel, cancel_read_scheduled_from_aio_thread)
{
    channel().executeInAioThreadSync(
        [this]()
        {
            givenScheduledRead();
            whenCancelledRead();
        });

    assertIoHasBeenProperlyCancelled();
}

TEST_F(StreamTransformingAsyncChannel, cancel_write_scheduled_from_non_aio_thread)
{
    givenScheduledWrite();
    whenCancelledWrite();
    assertIoHasBeenProperlyCancelled();
}

TEST_F(StreamTransformingAsyncChannel, cancel_write_scheduled_from_aio_thread)
{
    channel().executeInAioThreadSync(
        [this]()
        {
            givenScheduledWrite();
            whenCancelledWrite();
        });

    assertIoHasBeenProperlyCancelled();
}

TEST_F(
    StreamTransformingAsyncChannel,
    send_completion_is_reported_after_send_completion_on_raw_channel)
{
    whenSendSomeData();
    thenSendHasSucceeded();

    assertRawChannelHasCompletedSend();
}

//-------------------------------------------------------------------------------------------------

class StreamTransformingAsyncChannelIoErrors:
    public StreamTransformingAsyncChannel
{
public:
    StreamTransformingAsyncChannelIoErrors()
    {
        givenReflectingRawChannel();
    }

protected:
    void emulateReadTimeoutOnUnderlyingChannel()
    {
        rawDataChannel()->setReadErrorState(SystemError::timedOut);
    }

    void emulateSendTimeoutOnUnderlyingChannel()
    {
        rawDataChannel()->setSendErrorState(SystemError::timedOut);
    }

    void whenUnderlyingChannelIsFullyFunctionalAgain()
    {
        rawDataChannel()->setReadErrorState(std::nullopt);
        rawDataChannel()->setSendErrorState(std::nullopt);
    }

    void thenReadTimedoutHasBeenRaised()
    {
        nx::Buffer readBuffer;
        readBuffer.reserve(4*1024);

        nx::utils::promise<SystemError::ErrorCode> ioCompleted;
        channel().readSomeAsync(
            &readBuffer,
            [&ioCompleted](
                SystemError::ErrorCode sysErrorCode, std::size_t /*bytesRead*/)
            {
                ioCompleted.set_value(sysErrorCode);
            });
        ASSERT_EQ(SystemError::timedOut, ioCompleted.get_future().get());
    }

    void thenSendTimedoutHasBeenRaisedByUnderlyingChannel()
    {
        rawDataChannel()->waitForAnotherSendErrorReported();
    }
};

//TEST_F(StreamTransformingAsyncChannel, cancel_read_does_not_cancel_operations_required_by_write)

//TEST_F(StreamTransformingAsyncChannel, read_error)
//TEST_F(StreamTransformingAsyncChannel, read_eof)
//TEST_F(StreamTransformingAsyncChannel, write_error)
//TEST_F(StreamTransformingAsyncChannel, read_thirst)
//TEST_F(StreamTransformingAsyncChannel, write_thirst)
//TEST_F(StreamTransformingAsyncChannel, all_data_is_read_in_case_of_write_error)

TEST_F(StreamTransformingAsyncChannelIoErrors, read_timeout)
{
    emulateReadTimeoutOnUnderlyingChannel();
    thenReadTimedoutHasBeenRaised();

    whenUnderlyingChannelIsFullyFunctionalAgain();

    whenTransferredFiniteData();
    assertDataReadMatchesDataWritten();
}

TEST_F(StreamTransformingAsyncChannelIoErrors, send_timeout_on_underlying_channel)
{
    emulateSendTimeoutOnUnderlyingChannel();
    whenSendFiniteData();
    thenSendHasFailed();
}

//TEST_F(StreamTransformingAsyncChannelIoErrors, send_timeout_on_converted_data_send_queue_overflow)

//TEST_F(StreamTransformingAsyncChannel, removing_in_completion_handler)
//TEST_F(StreamTransformingAsyncChannel, raw_channel_io_method_calls_handler_within_method_call)

} // namespace test
} // namespace aio
} // namespace network
} // namespace nx
