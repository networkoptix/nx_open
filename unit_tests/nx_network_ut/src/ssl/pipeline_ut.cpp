#include <gtest/gtest.h>

#include <nx/network/ssl/ssl_pipeline.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/random.h>
#include <nx/utils/std/cpp14.h>

namespace nx {
namespace network {
namespace ssl {
namespace test {

class TestPipeline
{
public:
    TestPipeline(
        std::unique_ptr<utils::pipeline::TwoWayPipeline> clientPipeline,
        std::unique_ptr<utils::pipeline::TwoWayPipeline> serverPipeline)
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

    void insertBetweenClientAndServer(
        std::unique_ptr<utils::pipeline::AbstractOutputConverter> stream)
    {
        m_betweenClientAndServer = std::move(stream);

        m_clientPipeline->setOutput(m_betweenClientAndServer.get());
        m_betweenClientAndServer->setOutput(&m_clientToServerPipeline);
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
            else if (bytesWritten != utils::pipeline::StreamIoError::wouldBlock)
                break;

            QByteArray readBuffer;
            readBuffer.resize(msgLength);
            int bytesRead = m_serverPipeline->read(readBuffer.data(), readBuffer.size());
            if (bytesRead > 0)
                result += readBuffer.mid(0, bytesRead);
            else if (bytesRead == 0)
                ; // TODO
            else if (bytesRead != utils::pipeline::StreamIoError::wouldBlock)
                break;

            //if (m_clientPipeline->failed() || m_serverPipeline->failed())
            //    break;
        }

        return result;
    }

private:
    utils::pipeline::ReflectingPipeline m_clientToServerPipeline;
    utils::pipeline::ReflectingPipeline m_serverToClientPipeline;
    std::unique_ptr<utils::pipeline::TwoWayPipeline> m_clientPipeline;
    std::unique_ptr<utils::pipeline::TwoWayPipeline> m_serverPipeline;
    std::unique_ptr<utils::pipeline::AbstractOutputConverter> m_betweenClientAndServer;
    const int m_maxBytesToWrite;
};

/**
 * Invokes handler when pipelined wrapped writes any data to the output pipeline.
 */
class NotifyingTwoWayPipelineWrapper:
    public utils::pipeline::TwoWayPipeline
{
public:
    //using DataReadEventHandler = nx::utils::MoveOnlyFunc<void(void*, size_t)>;
    using DataWrittenEventHandler = nx::utils::MoveOnlyFunc<void(const void*, size_t)>;

    NotifyingTwoWayPipelineWrapper(
        std::unique_ptr<utils::pipeline::TwoWayPipeline> twoWayPipeline)
        :
        m_twoWayPipeline(std::move(twoWayPipeline))
    {
        using namespace std::placeholders;

        m_customOutput = makeCustomOutputPipeline(
            std::bind(&NotifyingTwoWayPipelineWrapper::writeCustom, this, _1, _2));
        m_twoWayPipeline->setOutput(m_customOutput.get());
    }

    virtual int read(void* data, size_t count) override
    {
        return m_twoWayPipeline->read(data, count);
    }

    virtual int write(const void* data, size_t count) override
    {
        return m_twoWayPipeline->write(data, count);
    }

    virtual void setInput(AbstractInput* input) override
    {
        m_twoWayPipeline->setInput(input);
    }

    //void setOnDataRead(DataReadEventHandler func)
    //{
    //    m_onDataRead = std::move(func);
    //}

    void setOnDataWritten(DataWrittenEventHandler func)
    {
        m_onDataWritten = std::move(func);
    }

private:
    //DataReadEventHandler m_onDataRead;
    DataWrittenEventHandler m_onDataWritten;
    std::unique_ptr<utils::pipeline::TwoWayPipeline> m_twoWayPipeline;
    std::unique_ptr<AbstractOutput> m_customOutput;

    int writeCustom(const void* data, size_t count)
    {
        const int bytesWritten = m_outputStream->write(data, count);
        if ((bytesWritten >= 0) && m_onDataWritten)
            m_onDataWritten(data, bytesWritten);
        return bytesWritten;
    }
};

class FailingOutputStream:
    public utils::pipeline::AbstractOutputConverter
{
public:
    int write(const void* data, size_t count)
    {
        m_buffer.assign((const char*)data, (const char*)data + count);
        // Corrupting buffer
        const auto pos = nx::utils::random::number<std::size_t>(0U, m_buffer.size());
        m_buffer[pos] = (char)nx::utils::random::number<int>();
        return m_outputStream->write(m_buffer.data(), m_buffer.size());
    }

private:
    std::vector<char> m_buffer;
};

//-------------------------------------------------------------------------------------------------
// Test fixture

class SslPipeline:
    public ::testing::Test
{
public:
    SslPipeline():
        m_serverPipeline(nullptr)
    {
        givenRandomData();
    }

protected:
    void givenRandomData()
    {
        m_inputData.resize(nx::utils::random::number<size_t>(1024, 2048));
        std::generate(m_inputData.data(), m_inputData.data() + m_inputData.size(), rand);
    }

    void givenEncodeDecodePipeline()
    {
        auto serverPipeline = createServerPipeline();
        auto notifyingPipeline =
            std::make_unique<NotifyingTwoWayPipelineWrapper>(std::move(serverPipeline));
        notifyingPipeline->setOnDataWritten(
            [this](const void* data, size_t count)
            {
                m_encodedData.append(static_cast<const char*>(data), count);
            });

        m_pipeline = std::make_unique<TestPipeline>(
            createClientPipeline(),
            std::move(notifyingPipeline));
    }

    void insertFailingElementIntoPipeChain()
    {
        m_pipeline->insertBetweenClientAndServer(
            std::make_unique<FailingOutputStream>());
    }

    void whenDataIsTransferredThroughPipeline()
    {
        m_outputData = m_pipeline->transferThrough(m_inputData);
    }

    void whenDataIsTransferredThroughPipelineInRandomSizeChunks()
    {
        constexpr int minChunkSize = 1;
        constexpr int maxChunkSize = 128;
        
        for (int pos = 0; pos < m_inputData.size();)
        {
            const auto bytesToWrite = std::min<int>(
                m_inputData.size() - pos,
                nx::utils::random::number(minChunkSize, maxChunkSize));
            m_outputData += m_pipeline->transferThrough(m_inputData.mid(pos, bytesToWrite));
            pos += bytesToWrite;
        }
    }

    void thenEncodedDataIsNotEmpty()
    {
        ASSERT_FALSE(m_encodedData.isEmpty());
    }

    void thenEncodedDataDiffersFromInitial()
    {
        ASSERT_NE(m_inputData, m_encodedData);
    }

    void thenOutputDataMatchesInput()
    {
        ASSERT_EQ(m_inputData, m_outputData);
    }

    void thenServerPipelineHasFailed()
    {
        ASSERT_TRUE(m_serverPipeline->eof());
        ASSERT_TRUE(m_serverPipeline->failed());
    }
    
    void thenPartialDataHasBeenReceived()
    {
        ASSERT_TRUE(m_inputData.startsWith(m_outputData));
    }

    std::unique_ptr<Pipeline> createClientPipeline()
    {
        return std::make_unique<ConnectingPipeline>();
    }

    std::unique_ptr<Pipeline> createServerPipeline()
    {
        auto serverPipeline = std::make_unique<AcceptingPipeline>();
        m_serverPipeline = serverPipeline.get();
        return serverPipeline;
    }

private:
    QByteArray m_inputData;
    QByteArray m_encodedData;
    QByteArray m_outputData;
    std::unique_ptr<TestPipeline> m_pipeline;
    AcceptingPipeline* m_serverPipeline;
};

//-------------------------------------------------------------------------------------------------
// Test cases

TEST_F(SslPipeline, data_is_actually_encoded)
{
    givenEncodeDecodePipeline();

    whenDataIsTransferredThroughPipeline();

    thenEncodedDataIsNotEmpty();
    thenEncodedDataDiffersFromInitial();
}

TEST_F(SslPipeline, data_stays_the_same_after_encoding_decoding)
{
    givenEncodeDecodePipeline();

    whenDataIsTransferredThroughPipeline();

    thenOutputDataMatchesInput();
}

TEST_F(SslPipeline, partitioning_input_data)
{
    givenEncodeDecodePipeline();

    whenDataIsTransferredThroughPipelineInRandomSizeChunks();

    thenOutputDataMatchesInput();
}

//TEST_F(SslPipeline, partitioning_intermediate_input_data)

//TEST_F(SslPipeline, partitioning_every_data)

TEST_F(SslPipeline, random_error_in_data)
{
    givenEncodeDecodePipeline();
    insertFailingElementIntoPipeChain();

    whenDataIsTransferredThroughPipeline();

    thenServerPipelineHasFailed();
    thenPartialDataHasBeenReceived();
}

//TEST_F(SslPipeline, eof)

//TEST_F(SslPipeline, handshake_error)

//TEST_F(SslPipeline, unexpected_intermediate_input_depletion)

//TEST_F(SslPipeline, read_thirsty_flag)

//TEST_F(SslPipeline, write_thirsty_flag)

} // namespace test
} // namespace ssl
} // namespace network
} // namespace nx
