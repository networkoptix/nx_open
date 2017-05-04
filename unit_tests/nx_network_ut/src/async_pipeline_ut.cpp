#include <gtest/gtest.h>

#include <nx/network/async_pipeline.h>
#include <nx/utils/pipeline.h>

namespace nx {
namespace network {
namespace test {

class AsyncBufferReader:
    public aio::BasicPollable
{
public:
    AsyncBufferReader(
        utils::pipeline::AbstractInput* input,
        utils::pipeline::AbstractOutput* output)
        :
        m_input(input),
        m_output(output)
    {
    }

    void readSomeAsync(
        nx::Buffer* const buffer,
        std::function<void(SystemError::ErrorCode, size_t)> handler)
    {
        NX_ASSERT(buffer->capacity() > buffer->size());
      
        post(
            [this, buffer, handler = std::move(handler)]()
            {
                int bytesRead = m_input->read(
                    buffer->data() + buffer->size(),
                    buffer->capacity());
                if (bytesRead > 0)
                    buffer->resize(buffer->size() + bytesRead);
                handler(
                    bytesRead >= 0 ? SystemError::noError : SystemError::connectionReset,
                    bytesRead);
            });
    }

    void sendAsync(
        const nx::Buffer& buffer,
        std::function<void(SystemError::ErrorCode, size_t)> handler)
    {
        post(
            [this, &buffer, handler = std::move(handler)]()
            {
                int bytesWritten = m_output->write(buffer.constData(), buffer.size());
                handler(
                    bytesWritten >= 0 ? SystemError::noError : SystemError::connectionReset,
                    bytesWritten);
            });
    }

private:
    utils::pipeline::AbstractInput* m_input;
    utils::pipeline::AbstractOutput* m_output;
};

//-------------------------------------------------------------------------------------------------
// Test fixture

const QByteArray kLeftData = "Hello, world";
const QByteArray kRightData = "Bye, cruel world";

class AsyncPipeline:
    public ::testing::Test
{
public:
    ~AsyncPipeline()
    {
        if (m_pipeline)
            m_pipeline->pleaseStopSync();
    }

protected:
    void establishPipeline()
    {
        m_leftSource.write(kLeftData.constData(), kLeftData.size());
        m_rightSource.write(kRightData.constData(), kRightData.size());

        m_leftFile = std::make_unique<AsyncBufferReader>(&m_leftSource, &m_leftDest);
        m_rightFile = std::make_unique<AsyncBufferReader>(&m_rightSource, &m_rightDest);

        m_pipeline = makeAsyncPipeline(m_leftFile.get(), m_rightFile.get());
    }

    void exchangeData()
    {
        utils::promise<void> done;
        m_pipeline->setOnDone([&done]() { done.set_value(); });
        m_pipeline->start();
        done.get_future().wait();
    }

    void assertDataExchangeWasSuccessful()
    {
        ASSERT_EQ(kLeftData, m_rightDest.internalBuffer());
        ASSERT_EQ(kRightData, m_leftDest.internalBuffer());
    }

private:
    utils::pipeline::ReflectingPipeline m_leftSource;
    utils::pipeline::ReflectingPipeline m_leftDest;
    utils::pipeline::ReflectingPipeline m_rightSource;
    utils::pipeline::ReflectingPipeline m_rightDest;
    std::unique_ptr<network::AbstractAsyncPipeline> m_pipeline;
    std::unique_ptr<AsyncBufferReader> m_leftFile;
    std::unique_ptr<AsyncBufferReader> m_rightFile;
};

//TEST_F(AsyncPipeline, data_is_passed_through)
//{
//    establishPipeline();
//    exchangeData();
//    assertDataExchangeWasSuccessful();
//}

//TEST_F(AsyncPipeline, cancellation)

} // namespace test
} // namespace network
} // namespace nx
