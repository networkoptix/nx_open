#include <gtest/gtest.h>

#include <nx/utils/system_error.h>

#include <udt/channel.h>

namespace test {

static constexpr int kIpVersion = AF_INET;

class UdpChannel:
    public ::testing::Test
{
protected:
    void givenChannelWithBrokenSocket()
    {
        createUdpSocket();

        m_channel = std::make_unique<::UdpChannel>(kIpVersion);
        ASSERT_TRUE(m_channel->open(m_udpSocket).ok());

        closeSocket();
    }

    void whenSend()
    {
        CPacket packet;
        packet.setLength(128);
        m_lastSendResult = m_channel->sendto(m_channel->getSockAddr(), packet);
    }

    void thenSendFailed()
    {
        ASSERT_FALSE(m_lastSendResult->ok());
    }

    void createUdpSocket()
    {
        m_udpSocket = ::socket(kIpVersion, SOCK_DGRAM, 0);
        ASSERT_NE(INVALID_UDP_SOCKET, m_udpSocket)
            << SystemError::getLastOSErrorText().toStdString();
    }

    void closeSocket()
    {
        #if defined(_WIN32)
            ::closesocket(m_udpSocket);
        #else
            ::close(m_udpSocket);
        #endif

        m_udpSocket = INVALID_UDP_SOCKET;
    }

private:
    std::optional<Result<int>> m_lastSendResult;
    std::unique_ptr<::UdpChannel> m_channel;
    UDPSOCKET m_udpSocket = INVALID_UDP_SOCKET;
};

TEST_F(UdpChannel, error_reported)
{
    givenChannelWithBrokenSocket();
    whenSend();
    thenSendFailed();
}

} // namespace test
