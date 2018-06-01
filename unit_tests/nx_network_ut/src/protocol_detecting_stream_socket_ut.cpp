#include <memory>

#include <gtest/gtest.h>

#include <nx/network/protocol_detecting_stream_socket.h>
#include <nx/network/test_support/buffer_socket.h>
#include <nx/utils/random.h>
#include <nx/utils/string.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx {
namespace network {
namespace test {

class ProtocolDetectingStreamSocket:
    public ::testing::Test
{
public:
    ProtocolDetectingStreamSocket()
    {
        generateMultipleProtocolsHeaders();
        transferHeaderOfRandomProtocol();
    }

    ~ProtocolDetectingStreamSocket()
    {
        if (m_detector)
            m_detector->pleaseStopSync();
        if (m_rawDataSource)
            m_rawDataSource->pleaseStopSync();
    }

protected:
    void whenReadSocketSynchronously()
    {
        std::array<char, 1024> buf;
        ASSERT_GT(m_detector->recv(buf.data(), buf.size(), 0), 0);
    }

    void assertProperProtocolIsDetected()
    {
        ASSERT_EQ(m_selectedProtocolId, m_selectedProtocolEventQueue.pop());
    }

    void assertSendFailsWith(SystemError::ErrorCode systemErrorCode)
    {
        std::array<char, 1024> buf;
        ASSERT_EQ(-1, m_detector->send(buf.data(), buf.size()));
        ASSERT_EQ(systemErrorCode, SystemError::getLastOSErrorCode());
    }

private:
    int m_prevAssignedProtocolNumber = 0;
    std::unique_ptr<network::ProtocolDetectingStreamSocket<AbstractStreamSocket>> m_detector;
    std::map<int /*protocolId*/, std::string> m_protocols;
    std::unique_ptr<AbstractStreamSocket> m_rawDataSource;
    nx::utils::SyncQueue<int> m_selectedProtocolEventQueue;
    int m_selectedProtocolId = -1;

    void generateMultipleProtocolsHeaders()
    {
        constexpr int protocolCount = 3;
        for (int i = 0; i < protocolCount; ++i)
        {
            auto protocolHeader = nx::utils::generateRandomName(
                nx::utils::random::number<int>(7, 13)).toStdString();
            m_protocols.emplace(++m_prevAssignedProtocolNumber, protocolHeader);
        }
    }

    void transferHeaderOfRandomProtocol()
    {
        auto& selectedProtocol = *m_protocols.begin();
        auto source = std::make_unique<BufferSocket>(selectedProtocol.second);
        m_detector =
            std::make_unique<network::ProtocolDetectingStreamSocket<AbstractStreamSocket>>(
                std::move(source));
        m_selectedProtocolId = selectedProtocol.first;

        registerProtocols();
    }

    void registerProtocols()
    {
        using namespace std::placeholders;

        for (const auto& protocol: m_protocols)
        {
            m_detector->registerProtocol(
                std::make_unique<FixedProtocolPrefixRule>(protocol.second),
                std::bind(&ProtocolDetectingStreamSocket::createProtocolHandler, this,
                    protocol.first, _1));
        }
    }

    std::unique_ptr<AbstractStreamSocket> createProtocolHandler(
        int protocolId,
        std::unique_ptr<AbstractStreamSocket> rawDataSource)
    {
        m_selectedProtocolEventQueue.push(protocolId);

        m_rawDataSource = std::move(rawDataSource);
        return std::make_unique<StreamSocketDelegate>(m_rawDataSource.get());
    }
};

TEST_F(ProtocolDetectingStreamSocket, sync_recv)
{
    whenReadSocketSynchronously();
    assertProperProtocolIsDetected();
}

TEST_F(
    ProtocolDetectingStreamSocket,
    sync_send_invoked_before_protocol_detection_results_in_error)
{
    assertSendFailsWith(SystemError::invalidData);
}

} // namespace test
} // namespace network
} // namespace nx
