// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/network/stun/async_client.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/std/future.h>
#include <nx/utils/sync_call.h>

#include "stun/stun_over_http_server_fixture.h"

namespace nx::network::http::server::test {

/**
 * Tests tunneling of STUN protocol into HTTP connection.
 */
class HttpTunnelingServer:
    public nx::network::stun::test::StunOverHttpServerFixture
{
protected:
    void whenEstablishConnection()
    {
        using namespace std::placeholders;

        HttpClient httpClient{ssl::kAcceptAnyCertificate};
        ASSERT_TRUE(
            httpClient.doUpgrade(
                tunnelUrl(),
                nx::network::stun::StunOverHttpServer::kStunProtocolName));
        ASSERT_EQ(
            nx::network::http::StatusCode::switchingProtocols,
            httpClient.response()->statusLine.statusCode);
        m_httpTunnelConnection = httpClient.takeSocket();
    }

    void whenSendStunRequest()
    {
        using namespace std::placeholders;

        ASSERT_TRUE(m_httpTunnelConnection->setNonBlockingMode(true));

        nx::network::stun::AsyncClient stunClient(std::move(m_httpTunnelConnection));
        auto stunClientGuard = nx::utils::makeScopeGuard(
            [&stunClient]() { stunClient.pleaseStopSync(); });

        auto request = prepareRequest();

        auto result = makeSyncCall<SystemError::ErrorCode, nx::network::stun::Message>(
            std::bind(&nx::network::stun::AsyncClient::sendRequest, &stunClient,
                request, _1, nullptr));
        ASSERT_EQ(SystemError::noError, std::get<0>(result));
        ASSERT_EQ(
            nx::network::stun::MessageClass::successResponse,
            std::get<1>(result).header.messageClass);
    }

    void thenStunMessageIsReceivedByServer()
    {
        popReceivedMessage();
    }

private:
    std::unique_ptr<AbstractStreamSocket> m_httpTunnelConnection;
};

TEST_F(HttpTunnelingServer, tunnel_is_established)
{
    givenTunnelingServer();

    whenEstablishConnection();
    whenSendStunRequest();

    thenStunMessageIsReceivedByServer();
}

} // namespace nx::network::http::server::test
