#include <gtest/gtest.h>

#include <nx/network/ssl/ssl_pipeline.h>
#include <nx/utils/std/cpp14.h>

namespace nx {
namespace network {
namespace ssl {
namespace test {

class BufferPipeline:
    public AbstractInput,
    public AbstractOutput
{
public:
    virtual int write(const void* data, size_t count) override
    {
        m_buffer.append(static_cast<const char*>(data), count);
        return count;
    }
    
    virtual int read(void* data, size_t count) override
    {
        const auto bytesToRead = std::min<size_t>(count, m_buffer.size());
        memcpy(data, m_buffer.data(), bytesToRead);
        m_buffer.remove(0, bytesToRead);
        return bytesToRead;
    }

private:
    QByteArray m_buffer;
};

class TestPipeline
{
public:
    TestPipeline(
        std::unique_ptr<Pipeline> clientPipeline,
        std::unique_ptr<Pipeline> serverPipeline)
        :
        m_clientPipeline(std::move(clientPipeline)),
        m_serverPipeline(std::move(serverPipeline)),
        m_maxBytesToWrite(100)
    {
        m_clientPipeline->setOutput(&m_clientToServerPipeline);
        m_serverPipeline->setInput(&m_clientToServerPipeline);

        m_serverPipeline->setOutput(&m_serverToClientPipeline);
        m_clientPipeline->setInput(&m_serverToClientPipeline);
    }

    QByteArray transferThrough(QByteArray dataToSend)
    {
        const int msgLength = dataToSend.size();

        QByteArray result;

        while (!dataToSend.isEmpty())
        {
            const int bytesToWrite = std::min<int>(m_maxBytesToWrite, dataToSend.size());

            const int bytesWritten = 
                m_clientPipeline->write(dataToSend.constData(), bytesToWrite);
            if (bytesWritten > 0)
                dataToSend.remove(0, bytesWritten);
            else if (bytesWritten == 0)
                ; // TODO
            else if (bytesWritten != StreamIoError::wouldBlock)
                return QByteArray();

            QByteArray readBuffer;
            readBuffer.resize(msgLength);
            int bytesRead = m_serverPipeline->read(readBuffer.data(), readBuffer.size());
            if (bytesRead > 0)
                result += readBuffer.mid(0, bytesRead);
            else if (bytesRead == 0)
                ; // TODO
            else if (bytesRead != StreamIoError::wouldBlock)
                return QByteArray();
        }

        return result;
    }

private:
    BufferPipeline m_clientToServerPipeline;
    BufferPipeline m_serverToClientPipeline;
    std::unique_ptr<Pipeline> m_clientPipeline;
    std::unique_ptr<Pipeline> m_serverPipeline;
    const int m_maxBytesToWrite;
};

class SslPipeline:
    public ::testing::Test
{
public:

protected:
    std::unique_ptr<TestPipeline> createFullPipeline()
    {
        return std::make_unique<TestPipeline>(createClientPipeline(), createServerPipeline());
    }

    std::unique_ptr<Pipeline> createClientPipeline()
    {
        return std::make_unique<ConnectingPipeline>();
    }

    std::unique_ptr<Pipeline> createServerPipeline()
    {
        return std::make_unique<AcceptingPipeline>();
    }
};

TEST_F(SslPipeline, common)
{
    QByteArray testData;
    testData.resize(1024);
    std::generate(testData.data(), testData.data() + testData.size(), rand);
    //testData = "Hello, world";

    auto pipeline = createFullPipeline();
    ASSERT_EQ(testData, pipeline->transferThrough(testData));
}

//TEST_F(SslPipeline, partitioning_input_data)
//TEST_F(SslPipeline, partitioning_intermediate_input_data)
//TEST_F(SslPipeline, partitioning_every_data)
//TEST_F(SslPipeline, handshake_error)
//TEST_F(SslPipeline, random_error_in_data)
//TEST_F(SslPipeline, unexpected_intermediate_input_depletion)
//TEST_F(SslPipeline, read_thirsty_flag)
//TEST_F(SslPipeline, write_thirsty_flag)

} // namespace test
} // namespace ssl
} // namespace network
} // namespace nx
