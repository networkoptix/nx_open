/**********************************************************
* Dec 31, 2015
* akolesnikov
***********************************************************/

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <nx/network/stun/message_dispatcher.h>
#include <nx/network/stun/udp_client.h>
#include <nx/network/stun/udp_server.h>

#include <utils/common/sync_call.h>


namespace nx {
namespace stun {
namespace test {

class StunUDP
:
    public ::testing::Test
{
public:
    StunUDP()
    :
        m_udpServer(m_messageDispatcher)
    {
        using namespace std::placeholders;
        m_messageDispatcher.registerDefaultRequestProcessor(
            std::bind(&StunUDP::onMessage, this, _1, _2));

        assert(m_udpServer.bind(SocketAddress(HostAddress::localhost, 0)));
        assert(m_udpServer.listen());
    }

    ~StunUDP()
    {
    }

    SocketAddress endpoint() const
    {
        return SocketAddress(
            HostAddress::localhost,
            m_udpServer.address().port);
    }

private:
    MessageDispatcher m_messageDispatcher;
    UDPServer m_udpServer;

    void onMessage(
        std::shared_ptr< AbstractServerConnection > connection,
        stun::Message message)
    {
        nx::stun::Message response(
            stun::Header(
                nx::stun::MessageClass::successResponse,
                nx::stun::bindingMethod,
                message.header.transactionId));
        connection->sendMessage(std::move(response));
    }
};

TEST_F(StunUDP, server_reports_port)
{
    ASSERT_NE(0, endpoint().port);
}

/** Common test.
    Sending request to the server, receiving response
*/
TEST_F(StunUDP, general)
{
    UDPClient client;
    nx::stun::Message requestMessage(
        stun::Header(
            nx::stun::MessageClass::request,
            nx::stun::bindingMethod));

    SystemError::ErrorCode errorCode = SystemError::noError;
    nx::stun::Message response;
    std::tie(errorCode, response) = makeSyncCall<SystemError::ErrorCode, Message>(
        std::bind(
            &UDPClient::sendRequest,
            &client,
            endpoint(),
            std::move(requestMessage),
            std::placeholders::_1));
    ASSERT_EQ(SystemError::noError, errorCode);
    ASSERT_EQ(requestMessage.header.transactionId, response.header.transactionId);

    client.pleaseStopSync();
}

/** Testing request retransmission by client.
    - sending request
    - ignoring request on server
    - waiting for retransmission
    - server sends response
    - checking that client received response
*/
TEST_F(StunUDP, client_retransmits_general)
{
}

/** Checking that client reports failure after sending maximum retransmissions allowed.
    - sending request
    - ignoring all request on server
    - counting retransmission issued by a client
    - waiting for client to report failure
*/
TEST_F(StunUDP, client_retransmits_max_retransmits)
{
}

/** Checking that client reports error to waiters if removed before receiving response */
TEST_F(StunUDP, client_cancellation)
{
}

}   //test
}   //stun
}   //nx
