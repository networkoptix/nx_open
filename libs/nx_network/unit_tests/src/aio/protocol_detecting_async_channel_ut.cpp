#include <future>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <nx/network/aio/async_channel_adapter.h>
#include <nx/network/aio/protocol_detecting_async_channel.h>
#include <nx/network/aio/test/aio_test_async_channel.h>
#include <nx/network/protocol_detector.h>
#include <nx/utils/byte_stream/pipeline.h>
#include <nx/utils/random.h>
#include <nx/utils/string.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx {
namespace network {
namespace aio {
namespace test {

class ProtocolDetectingAsyncChannel:
    public ::testing::Test
{
public:
    ProtocolDetectingAsyncChannel()
    {
        auto rawDataChannel = std::make_unique<AsyncChannel>(
            &m_rawDataSource,
            &m_sentDataBuffer,
            AsyncChannel::InputDepletionPolicy::sendConnectionReset);
        m_rawDataChannel = rawDataChannel.get();

        m_protocolDetectingChannel =
            std::make_unique<aio::ProtocolDetectingAsyncChannel>(std::move(rawDataChannel));
    }

    ~ProtocolDetectingAsyncChannel()
    {
        if (m_protocolDetectingChannel)
        {
            m_protocolDetectingChannel->pleaseStopSync();
            m_protocolDetectingChannel.reset();
        }
    }

protected:
    void registerDefaultDelegate()
    {
        using namespace std::placeholders;

        m_protocolDetectingChannel->registerDefaultProtocol(
            std::bind(&ProtocolDetectingAsyncChannel::createDelegate, this,
                m_defaultDelegateId, _1));
    }

    void registerMultipleProtocols()
    {
        using namespace std::placeholders;

        constexpr int protocolCount = 3;
        for (int protoId = m_defaultDelegateId + 1;
            protoId < m_defaultDelegateId + 1 + protocolCount;
            ++protoId)
        {
            m_registeredProtocols.push_back(std::make_tuple(
                protoId,
                lm("p%1").args(protoId).toStdString()));

            m_protocolDetectingChannel->registerProtocol(
                std::make_unique<FixedProtocolPrefixRule>(
                    std::get<1>(m_registeredProtocols.back()).c_str()),
                std::bind(&ProtocolDetectingAsyncChannel::createDelegate, this, protoId, _1));
        }
    }

    void givenBrokenDataSource()
    {
        m_rawDataChannel->setReadErrorState(SystemError::connectionReset);
    }

    void whenReceiveRandomBytes()
    {
        auto randomBytes = nx::utils::generateRandomName(128);
        m_rawDataSource.write(randomBytes.constData(), randomBytes.size());

        readSynchronously();
    }

    void whenSendSomeBytes()
    {
        using namespace std::placeholders;

        m_sendBuffer = nx::utils::generateRandomName(128);
        m_protocolDetectingChannel->sendAsync(
            m_sendBuffer,
            std::bind(&ProtocolDetectingAsyncChannel::saveSendResult, this, _1, _2));
    }

    void whenReceiveSomeProtocolHeader()
    {
        const auto selectedProtocol =
            nx::utils::random::choice(m_registeredProtocols);
        const auto selectedProtocolHeader = std::get<1>(selectedProtocol);
        m_selectedProtocolId = std::get<0>(selectedProtocol);
        m_rawDataSource.write(selectedProtocolHeader.data(), selectedProtocolHeader.size());
        m_dataSent += selectedProtocolHeader.c_str();

        readSynchronously();
    }

    void whenReadAsync()
    {
        m_readBuffer.reserve(256);
        m_protocolDetectingChannel->readSomeAsync(
            &m_readBuffer,
            [this](
                SystemError::ErrorCode systemErrorCode, size_t /*bytesRead*/)
            {
                if (systemErrorCode == SystemError::noError)
                {
                    m_dataRead.push_back(m_readBuffer);
                    m_readBuffer.clear();
                }

                m_recvResultQueue.push(systemErrorCode);
            });
    }

    void thenRecvSucceeded()
    {
        ASSERT_EQ(SystemError::noError, m_recvResultQueue.pop());
    }

    void thenRecvFailed()
    {
        ASSERT_NE(SystemError::noError, m_recvResultQueue.pop());
    }

    void thenDefaultDelegateIsSelected()
    {
        ASSERT_EQ(m_defaultDelegateId, m_delegateCreatedEventQueue.pop());
    }

    void thenCorrespondingProtocolIsDetected()
    {
        ASSERT_EQ(m_selectedProtocolId, m_delegateCreatedEventQueue.pop());
    }

    void thenSelectedProtocolHandlerReceivesAllSourceData()
    {
        ASSERT_EQ(m_dataRead, m_dataSent);
    }

    void thenIoErrorIsReported()
    {
        ASSERT_NE(SystemError::noError, m_sendResult.pop());
    }

private:
    int m_defaultDelegateId = 0;
    AsyncChannel* m_rawDataChannel = nullptr;
    std::unique_ptr<aio::ProtocolDetectingAsyncChannel> m_protocolDetectingChannel;
    nx::utils::bstream::Pipe m_rawDataSource;
    nx::utils::bstream::Pipe m_sentDataBuffer;
    nx::utils::SyncQueue<int /*delegateId*/> m_delegateCreatedEventQueue;
    nx::utils::SyncQueue<SystemError::ErrorCode> m_sendResult;
    nx::utils::SyncQueue<SystemError::ErrorCode> m_recvResultQueue;
    nx::Buffer m_sendBuffer;
    std::vector<std::tuple<int /*id*/, std::string /*header*/>> m_registeredProtocols;
    int m_selectedProtocolId = -1;
    nx::Buffer m_dataRead;
    nx::Buffer m_dataSent;
    nx::Buffer m_readBuffer;

    std::unique_ptr<AbstractAsyncChannel> createDelegate(
        int delegateId,
        std::unique_ptr<AbstractAsyncChannel> rawDataSource)
    {
        m_delegateCreatedEventQueue.push(delegateId);
        return makeAsyncChannelAdapter(std::move(rawDataSource));
    }

    void readSynchronously()
    {
        whenReadAsync();
        thenRecvSucceeded();
    }

    void saveSendResult(SystemError::ErrorCode sysErrorCode, size_t /*bytesSent*/)
    {
        m_sendResult.push(sysErrorCode);
    }
};

TEST_F(ProtocolDetectingAsyncChannel, default_and_only_delegate_is_selected)
{
    registerDefaultDelegate();
    whenReceiveRandomBytes();
    thenDefaultDelegateIsSelected();
}

TEST_F(ProtocolDetectingAsyncChannel, send_before_detecting_protocol_results_in_an_error)
{
    registerDefaultDelegate();
    whenSendSomeBytes();
    thenIoErrorIsReported();
}

TEST_F(ProtocolDetectingAsyncChannel, matching_protocol_delegate_is_selected)
{
    registerMultipleProtocols();
    whenReceiveSomeProtocolHeader();
    thenCorrespondingProtocolIsDetected();
}

TEST_F(ProtocolDetectingAsyncChannel, protocol_detecting_data_passed_to_the_selected_delegate)
{
    registerMultipleProtocols();
    whenReceiveSomeProtocolHeader();
    thenSelectedProtocolHandlerReceivesAllSourceData();
}

TEST_F(ProtocolDetectingAsyncChannel, data_source_io_error_is_reported)
{
    givenBrokenDataSource();
    whenReadAsync();
    thenRecvFailed();
}

} // namespace test
} // namespace aio
} // namespace network
} // namespace nx
