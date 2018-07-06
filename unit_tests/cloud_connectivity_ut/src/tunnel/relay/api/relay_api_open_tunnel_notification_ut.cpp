#include <gtest/gtest.h>

#include <nx/network/cloud/tunnel/relay/api/relay_api_open_tunnel_notification.h>
#include <nx/utils/string.h>

namespace nx {
namespace cloud {
namespace relay {
namespace api {
namespace test {

class OpenTunnelNotification:
    public ::testing::Test
{
protected:
    void generateRandomData()
    {
        m_original.setClientEndpoint(nx::network::SocketAddress(nx::network::HostAddress::localhost, 12345));
        m_original.setClientPeerName(nx::utils::generateRandomName(7));
    }

    void givenNotificationWithEmptyPeerName()
    {
        generateRandomData();
        m_original.setClientPeerName(nx::String());
    }

    void serialize()
    {
        m_serialized = m_original.toHttpMessage().toString();
    }

    void parse()
    {
        nx::network::http::Message message(nx::network::http::MessageType::request);
        ASSERT_TRUE(message.request->parse(m_serialized));
        ASSERT_TRUE(m_parsed.parse(message));
    }

    void assertParsedMatchesOriginal()
    {
        ASSERT_EQ(m_parsed.clientEndpoint().toString(), m_original.clientEndpoint().toString());
        ASSERT_EQ(m_parsed.clientPeerName(), m_original.clientPeerName());
    }

private:
    api::OpenTunnelNotification m_original;
    nx::Buffer m_serialized;
    api::OpenTunnelNotification m_parsed;
};

TEST_F(OpenTunnelNotification, parse_serialize_reversible)
{
    generateRandomData();

    serialize();
    parse();

    assertParsedMatchesOriginal();
}

TEST_F(OpenTunnelNotification, empty_client_peer_name_supported)
{
    givenNotificationWithEmptyPeerName();

    serialize();
    parse();

    assertParsedMatchesOriginal();
}

} // namespace test
} // namespace api
} // namespace relay
} // namespace cloud
} // namespace nx
