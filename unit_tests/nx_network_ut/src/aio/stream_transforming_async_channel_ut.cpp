#include <memory>

#include <gtest/gtest.h>

#include <nx/network/aio/stream_transforming_async_channel.h>
#include <nx/network/aio/test/aio_test_async_channel.h>

namespace nx {
namespace network {
namespace aio {
namespace test {

class Converter:
    public utils::pipeline::Converter
{
public:
    int write(const void* data, size_t count)
    {
        return m_outputStream->write(data, count);
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
            data,
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
        m_channel(channel)
    {
    }

    void start()
    {
        scheduleNextRead();
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
            m_dataRead.push_back(m_readBuffer);
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
        m_converter(nullptr)
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

    void whenTransferringFiniteData()
    {
        m_inputData.resize(1024);
        std::generate(m_inputData.begin(), m_inputData.end(), rand);
        m_expectedOutputData = m_inputData;

        transferData();
    }

    void assertDataReadMatchesDataWritten()
    {
        ASSERT_EQ(m_expectedOutputData, m_outputData);
    }

private:
    std::unique_ptr<aio::StreamTransformingAsyncChannel> m_channel;
    AsyncChannel* m_rawDataChannel;
    Converter* m_converter;
    utils::pipeline::ReflectingPipeline m_reflectingPipeline;
    nx::Buffer m_inputData;
    nx::Buffer m_expectedOutputData;
    nx::Buffer m_outputData;

    void createTransformingChannel()
    {
        auto rawDataChannel = std::make_unique<AsyncChannel>(
            &m_reflectingPipeline,
            &m_reflectingPipeline,
            AsyncChannel::InputDepletionPolicy::ignore);
        m_rawDataChannel = rawDataChannel.get();

        auto converter = std::make_unique<Converter>();
        m_converter = converter.get();

        m_channel = std::make_unique<aio::StreamTransformingAsyncChannel>(
            std::move(rawDataChannel),
            std::move(converter));
    }

    void transferData()
    {
        ChannelWriter writer(m_channel.get());
        ChannelReader reader(m_channel.get());
        reader.start();

        ASSERT_EQ(SystemError::noError, writer.writeSync(m_inputData));

        reader.waitEof();
        m_outputData = reader.dataRead();
    }
};

TEST_F(StreamTransformingAsyncChannel, read_write)
{
    givenReflectingRawChannel();
    whenTransferringFiniteData();
    assertDataReadMatchesDataWritten();
}

//TEST_F(StreamTransformingAsyncChannel, read_error)
//TEST_F(StreamTransformingAsyncChannel, read_eof)
//TEST_F(StreamTransformingAsyncChannel, write_error)
//TEST_F(StreamTransformingAsyncChannel, read_thirst)
//TEST_F(StreamTransformingAsyncChannel, write_thirst)
//TEST_F(StreamTransformingAsyncChannel, all_data_is_read_in_case_of_write_error)
//TEST_F(StreamTransformingAsyncChannel, read_timeout)
//TEST_F(StreamTransformingAsyncChannel, write_timeout)
//TEST_F(StreamTransformingAsyncChannel, removing_in_completion_handler)

} // namespace test
} // namespace aio
} // namespace network
} // namespace nx
