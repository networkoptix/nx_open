#include <gtest/gtest.h>

#include <limits>

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
        std::unique_ptr<utils::bstream::AbstractTwoWayConverter> clientPipeline,
        std::unique_ptr<utils::bstream::AbstractTwoWayConverter> serverPipeline)
        :
        m_clientPipeline(std::move(clientPipeline)),
        m_serverPipeline(std::move(serverPipeline)),
        m_maxBytesToWrite(std::numeric_limits<decltype(m_maxBytesToWrite)>::max()),
        m_clientToServerTotalBytesThrough(-1),
        m_serverToClientTotalBytesThrough(-1)
    {
        m_clientPipeline->setOutput(&m_clientToServerPipeline);
        m_serverPipeline->setInput(&m_clientToServerPipeline);

        m_serverPipeline->setOutput(&m_serverToClientPipeline);
        m_clientPipeline->setInput(&m_serverToClientPipeline);
    }

    void setTransferWindowSize(std::size_t windowSize)
    {
        m_clientToServerPipeline.setMaxBufferSize(windowSize);
        m_serverToClientPipeline.setMaxBufferSize(windowSize);
    }

    void insertBetweenClientAndServer(
        std::unique_ptr<utils::bstream::AbstractOutputConverter> stream)
    {
        m_betweenClientAndServer = std::move(stream);

        m_clientPipeline->setOutput(m_betweenClientAndServer.get());
        m_betweenClientAndServer->setOutput(&m_clientToServerPipeline);
    }

    QByteArray transferThrough(QByteArray dataToSend)
    {
        bool senderActive = true;
        bool receiverActive = true;

        const int msgLength = dataToSend.size();

        QByteArray result;

        while (!dataToSend.isEmpty())
        {
            if (!senderActive && !receiverActive)
                break;

            if (m_clientToServerPipeline.totalBytesThrough() == m_clientToServerTotalBytesThrough &&
                m_serverToClientPipeline.totalBytesThrough() == m_serverToClientTotalBytesThrough)
            {
                // On error in handshake data SSL may not report error but keep asking for data.
                // Another side will also be asking for data and as a result pipe will stuck.
                break;
            }

            m_clientToServerTotalBytesThrough = m_clientToServerPipeline.totalBytesThrough();
            m_serverToClientTotalBytesThrough = m_serverToClientPipeline.totalBytesThrough();

            if (senderActive)
            {
                const int bytesToWrite = std::min<int>(m_maxBytesToWrite, dataToSend.size());
                const int bytesWritten =
                    m_clientPipeline->write(dataToSend.constData(), bytesToWrite);
                if (bytesWritten > 0)
                    dataToSend.remove(0, bytesWritten);
                else if (bytesWritten == 0)
                    ; // TODO
                else if (bytesWritten != utils::bstream::StreamIoError::wouldBlock)
                    senderActive = false;
            }

            if (receiverActive)
            {
                QByteArray readBuffer;
                readBuffer.resize(msgLength);
                int bytesRead = m_serverPipeline->read(readBuffer.data(), readBuffer.size());
                if (bytesRead > 0)
                    result += readBuffer.mid(0, bytesRead);
                else if (bytesRead == 0)
                    ; // TODO
                else if (bytesRead != utils::bstream::StreamIoError::wouldBlock)
                    receiverActive = false;
            }
        }

        return result;
    }

private:
    utils::bstream::Pipe m_clientToServerPipeline;
    utils::bstream::Pipe m_serverToClientPipeline;
    std::unique_ptr<utils::bstream::AbstractTwoWayConverter> m_clientPipeline;
    std::unique_ptr<utils::bstream::AbstractTwoWayConverter> m_serverPipeline;
    std::unique_ptr<utils::bstream::AbstractOutputConverter> m_betweenClientAndServer;
    const int m_maxBytesToWrite;
    std::size_t m_clientToServerTotalBytesThrough;
    std::size_t m_serverToClientTotalBytesThrough;
};

/**
 * Invokes handler when pipelined wrapped writes any data to the output pipeline.
 */
class NotifyingTwoWayPipelineWrapper:
    public utils::bstream::AbstractTwoWayConverter
{
public:
    using DataWrittenEventHandler = nx::utils::MoveOnlyFunc<void(const void*, size_t)>;

    NotifyingTwoWayPipelineWrapper(
        std::unique_ptr<utils::bstream::AbstractTwoWayConverter> twoWayPipeline)
        :
        m_twoWayPipeline(std::move(twoWayPipeline))
    {
        using namespace std::placeholders;

        m_customOutput = makeCustomOutput(
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

    void setOnDataWritten(DataWrittenEventHandler func)
    {
        m_onDataWritten = std::move(func);
    }

private:
    //DataReadEventHandler m_onDataRead;
    DataWrittenEventHandler m_onDataWritten;
    std::unique_ptr<utils::bstream::AbstractTwoWayConverter> m_twoWayPipeline;
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
    public utils::bstream::AbstractOutputConverter
{
public:
    int write(const void* data, size_t count)
    {
        m_buffer.assign((const char*)data, (const char*)data + count);
        // Corrupting buffer
        const auto pos = nx::utils::random::number<std::size_t>(0U, m_buffer.size()-1);
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
        m_clientPipeline(nullptr),
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

    void whenAnySideHasBeenShutDown()
    {
        m_clientPipeline->shutdown();
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

    void thenBothSidesReportEof()
    {
        ASSERT_TRUE(m_clientPipeline->eof());
        ASSERT_TRUE(m_serverPipeline->eof());
    }

    void setTransferWindowSize(std::size_t windowSize)
    {
        m_pipeline->setTransferWindowSize(windowSize);
    }

    std::unique_ptr<Pipeline> createClientPipeline()
    {
        auto pipeline = std::make_unique<ConnectingPipeline>();
        m_clientPipeline = pipeline.get();
        return std::move(pipeline);
    }

    std::unique_ptr<Pipeline> createServerPipeline()
    {
        auto serverPipeline = std::make_unique<AcceptingPipeline>();
        m_serverPipeline = serverPipeline.get();
        return std::move(serverPipeline);
    }

    ConnectingPipeline* clientPipeline()
    {
        return m_clientPipeline;
    }

    AcceptingPipeline* serverPipeline()
    {
        return m_serverPipeline;
    }

private:
    QByteArray m_inputData;
    QByteArray m_encodedData;
    QByteArray m_outputData;
    std::unique_ptr<TestPipeline> m_pipeline;
    ConnectingPipeline* m_clientPipeline;
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

    // On error in handshake data SSL may not report error but keep asking for data.
    // Another side will also be asking for data and as a result pipe will stuck.
    // So, we cannot always detect error.
    thenPartialDataHasBeenReceived();
}

TEST_F(SslPipeline, shutdown)
{
    givenEncodeDecodePipeline();

    whenDataIsTransferredThroughPipeline();
    whenAnySideHasBeenShutDown();
    whenDataIsTransferredThroughPipeline();

    thenBothSidesReportEof();
}

//TEST_F(SslPipeline, incompatible_algorithm)

class SslPipelineThirstyFlags:
    public SslPipeline
{
public:

protected:
    void givenPipelineReadyForIo()
    {
        givenEncodeDecodePipeline();
        setTransferWindowSize(1024);
        // Performing handshake.
        whenDataIsTransferredThroughPipeline();
    }

    void testReadThirstyFlag(ssl::Pipeline* one, ssl::Pipeline* another)
    {
        std::array<char, 1024> buf;

        ASSERT_FALSE(one->isReadThirsty());
        one->read(buf.data(), buf.size());
        ASSERT_TRUE(one->isReadThirsty());

        another->write(buf.data(), buf.size());
        one->read(buf.data(), buf.size());
        ASSERT_FALSE(one->isReadThirsty());
    }

    void testWriteThirstyFlag(ssl::Pipeline* one, ssl::Pipeline* another)
    {
        std::array<char, 1024> buf;

        ASSERT_FALSE(one->isWriteThirsty());

        int result = 0;
        for (;;)
        {
            result = one->write(buf.data(), buf.size());
            if (result < 0)
                break;
        }
        ASSERT_EQ(utils::bstream::StreamIoError::wouldBlock, result);

        ASSERT_TRUE(one->isWriteThirsty());

        ASSERT_GT(another->read(buf.data(), buf.size()), 0);
        ASSERT_GT(one->write(buf.data(), buf.size()), 0);
        ASSERT_FALSE(one->isWriteThirsty());
    }
};

TEST_F(SslPipelineThirstyFlags, read_thirsty)
{
    givenPipelineReadyForIo();

    testReadThirstyFlag(clientPipeline(), serverPipeline());
    testReadThirstyFlag(serverPipeline(), clientPipeline());
}

TEST_F(SslPipelineThirstyFlags, write_thirsty)
{
    givenPipelineReadyForIo();

    testWriteThirstyFlag(clientPipeline(), serverPipeline());
    testWriteThirstyFlag(serverPipeline(), clientPipeline());
}

} // namespace test
} // namespace ssl
} // namespace network
} // namespace nx
