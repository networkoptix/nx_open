#pragma once

#include <gtest/gtest.h>

#include <nx/network/cloud/tunnel/relay/api/relay_api_client.h>
#include <nx/network/http/server/http_stream_socket_server.h>
#include <nx/cloud/relaying/listening_peer_pool.h>
#include <nx/utils/string.h>
#include <nx/utils/thread/sync_queue.h>

#include "basic_component_test.h"

namespace nx::cloud::relay::test {

template<typename RelayMethodTypeSet>
class RelayMethodAcceptance:
    public ::testing::Test,
    public BasicComponentTest
{
    using ClientSideApiClient = typename RelayMethodTypeSet::ClientSideApiClient;
    using ServerSideApiClient = typename RelayMethodTypeSet::ServerSideApiClient;

public:
    RelayMethodAcceptance():
        m_serverPeerName(nx::utils::generateRandomName(7).toStdString())
    {
    }

    ~RelayMethodAcceptance()
    {
        if (m_clientSideApiClient)
            m_clientSideApiClient->pleaseStopSync();
        if (m_serverSideApiClient)
            m_serverSideApiClient->pleaseStopSync();

        while (!m_serverTunnelResults.empty())
        {
            auto data = m_serverTunnelResults.pop();
            if (data.connection)
                data.connection->pleaseStopSync();
        }

        while (!m_clientTunnelResults.empty())
        {
            auto data = m_clientTunnelResults.pop();
            if (data.connection)
                data.connection->pleaseStopSync();
        }

        if (m_prevServerTunnelResult.connection)
            m_prevServerTunnelResult.connection->pleaseStopSync();

        if (m_prevClientTunnelResult.connection)
            m_prevClientTunnelResult.connection->pleaseStopSync();
    }

protected:
    virtual void SetUp() override
    {
        addRelayInstance();

        m_clientSideApiClient = std::make_unique<ClientSideApiClient>(
            relay().basicUrl(), nullptr);

        m_serverSideApiClient = std::make_unique<ServerSideApiClient>(
            relay().basicUrl(), nullptr);
    }

    //---------------------------------------------------------------------------------------------
    // Server.

    void whenInitiateServerTunnel()
    {
        using namespace std::placeholders;

        m_serverSideApiClient->beginListening(
            m_serverPeerName,
            std::bind(&RelayMethodAcceptance::saveServerTunnelResult, this,
                _1, _2, _3));
    }

    void thenServerTunnelSucceeded()
    {
        m_prevServerTunnelResult = m_serverTunnelResults.pop();
        ASSERT_EQ(api::ResultCode::ok, m_prevServerTunnelResult.resultCode);
    }

    void andServerIsOnlineOnRelay()
    {
        while (!relay().moduleInstance()->listeningPeerPool().isPeerOnline(m_serverPeerName))
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    //---------------------------------------------------------------------------------------------
    // Client.

    void whenInitiateClientTunnel()
    {
        using namespace std::placeholders;

        m_clientSideApiClient->startSession(
            "sessionId",
            m_serverPeerName,
            std::bind(&RelayMethodAcceptance::onCreateClientSessionCompleted, this, _1, _2));
    }

    void thenClientTunnelSucceeded()
    {
        m_prevClientTunnelResult = m_clientTunnelResults.pop();
        ASSERT_EQ(api::ResultCode::ok, m_prevClientTunnelResult.resultCode);
    }

    //---------------------------------------------------------------------------------------------

    void givenListeningServer()
    {
        whenInitiateServerTunnel();

        thenServerTunnelSucceeded();
        andServerIsOnlineOnRelay();
    }

    void whenClientConnectedToServer()
    {
        whenInitiateClientTunnel();
        thenClientTunnelSucceeded();
    }

    void whenClientRequestsConnectionUsingUnknownSessionId()
    {
        m_clientSideApiClient->openConnectionToTheTargetHost(
            "unknownSessionId",
            [this](
                api::ResultCode resultCode,
                std::unique_ptr<network::AbstractStreamSocket> connection)
            {
                m_clientTunnelResults.push({resultCode, std::move(connection)});
            });
    }

    void thenOpenTunnelNotificationIsSentThroughServerConnection()
    {
        auto httpPipe = std::make_unique<network::http::AsyncMessagePipeline>(
            std::exchange(m_prevServerTunnelResult.connection, nullptr));

        std::promise<std::tuple<
            std::unique_ptr<network::AbstractStreamSocket>, network::http::Message>
        > messageReceived;
        httpPipe->setMessageHandler(
            [httpPipe = httpPipe.get(), &messageReceived](
                network::http::Message message)
            {
                messageReceived.set_value({httpPipe->takeSocket(), std::move(message)});
            });
        httpPipe->startReadingConnection();

        auto connectionAndMessage = messageReceived.get_future().get();
        m_prevServerTunnelResult.connection =
            std::move(std::get<0>(connectionAndMessage));

        httpPipe->pleaseStopSync();
    }

    void thenCanExchangeDataBetweenServerAndClient()
    {
        exchangeSomeData(
            m_prevClientTunnelResult.connection.get(),
            m_prevServerTunnelResult.connection.get());
    }

    void thenRequestFailedWith(api::ResultCode expectedResultCode)
    {
        m_prevClientTunnelResult = m_clientTunnelResults.pop();
        ASSERT_EQ(expectedResultCode, m_prevClientTunnelResult.resultCode);
    }

private:
    struct ServerTunnelResult
    {
        api::ResultCode resultCode;
        api::BeginListeningResponse response;
        std::unique_ptr<network::AbstractStreamSocket> connection;

        ServerTunnelResult() = default;
    };

    struct ClientTunnelResult
    {
        api::ResultCode resultCode;
        std::unique_ptr<network::AbstractStreamSocket> connection;

        ClientTunnelResult() = default;
    };

    const std::string m_serverPeerName;
    std::unique_ptr<ClientSideApiClient> m_clientSideApiClient;
    std::unique_ptr<ServerSideApiClient> m_serverSideApiClient;

    nx::utils::SyncQueue<ServerTunnelResult> m_serverTunnelResults;
    ServerTunnelResult m_prevServerTunnelResult;

    nx::utils::SyncQueue<ClientTunnelResult> m_clientTunnelResults;
    ClientTunnelResult m_prevClientTunnelResult;

    void saveServerTunnelResult(
        api::ResultCode resultCode,
        api::BeginListeningResponse response,
        std::unique_ptr<network::AbstractStreamSocket> connection)
    {
        m_serverTunnelResults.push(
            {resultCode, std::move(response), std::move(connection)});
    }

    void onCreateClientSessionCompleted(
        api::ResultCode resultCode,
        api::CreateClientSessionResponse response)
    {
        using namespace std::placeholders;

        if (resultCode != api::ResultCode::ok)
        {
            m_clientTunnelResults.push({ resultCode, nullptr });
            return;
        }

        m_clientSideApiClient->openConnectionToTheTargetHost(
            response.sessionId,
            std::bind(&RelayMethodAcceptance::saveClientTunnelResult, this,
                _1, _2));
    }

    void saveClientTunnelResult(
        api::ResultCode resultCode,
        std::unique_ptr<network::AbstractStreamSocket> connection)
    {
        m_clientTunnelResults.push({ resultCode, std::move(connection) });
    }

    void exchangeSomeData(
        network::AbstractStreamSocket* one,
        network::AbstractStreamSocket* two)
    {
        ASSERT_TRUE(one->setNonBlockingMode(false));
        ASSERT_TRUE(two->setNonBlockingMode(false));

        sendData(one, two);
        sendData(two, one);
    }

    void sendData(
        network::AbstractStreamSocket* from,
        network::AbstractStreamSocket* to)
    {
        constexpr int bufSize = 256;

        std::array<char, bufSize> sendBuf;
        std::generate(sendBuf.begin(), sendBuf.end(), &rand);
        ASSERT_EQ(
            (int)sendBuf.size(),
            from->send(sendBuf.data(), (unsigned int)sendBuf.size()));

        std::array<char, bufSize> recvBuf;
        ASSERT_EQ(
            (int)sendBuf.size(),
            to->recv(recvBuf.data(), (unsigned int)recvBuf.size(), 0));

        ASSERT_EQ(sendBuf, recvBuf);
    }
};

TYPED_TEST_CASE_P(RelayMethodAcceptance);

//-------------------------------------------------------------------------------------------------

TYPED_TEST_P(RelayMethodAcceptance, server_can_establish_tunnel)
{
    this->whenInitiateServerTunnel();
    this->thenServerTunnelSucceeded();
}

TYPED_TEST_P(RelayMethodAcceptance, client_can_establish_tunnel)
{
    this->givenListeningServer();

    this->whenInitiateClientTunnel();
    this->thenClientTunnelSucceeded();
}

TYPED_TEST_P(RelayMethodAcceptance, client_and_server_can_exchange_data)
{
    this->givenListeningServer();

    this->whenClientConnectedToServer();
    this->thenOpenTunnelNotificationIsSentThroughServerConnection();

    this->thenCanExchangeDataBetweenServerAndClient();
}

TYPED_TEST_P(RelayMethodAcceptance, unknown_session_id_is_handled_properly)
{
    this->whenClientRequestsConnectionUsingUnknownSessionId();
    this->thenRequestFailedWith(api::ResultCode::notFound);
}

REGISTER_TYPED_TEST_CASE_P(RelayMethodAcceptance,
    server_can_establish_tunnel,
    client_can_establish_tunnel,
    client_and_server_can_exchange_data,
    unknown_session_id_is_handled_properly);

} // namespace nx::cloud::relay::test
