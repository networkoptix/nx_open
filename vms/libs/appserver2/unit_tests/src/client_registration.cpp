#include <gtest/gtest.h>

#include <nx/network/http/http_async_client.h>
#include <nx/network/url/url_builder.h>
#include <nx/network/websocket/websocket_handshake.h>
#include <nx/utils/test_support/module_instance_launcher.h>
#include <nx/utils/test_support/test_with_temporary_directory.h>
#include <nx/utils/thread/sync_queue.h>

#include <api/runtime_info_manager.h>
#include <nx/p2p/p2p_connection.h>
#include <nx/p2p/p2p_message_bus.h>
#include <test_support/appserver2_process.h>

namespace ec2::test {

class ClientRegistration:
    public ::testing::Test,
    public nx::utils::test::TestWithTemporaryDirectory
{
public:
    ClientRegistration():
        nx::utils::test::TestWithTemporaryDirectory("appserver2_ut", ""),
        m_clientId(QnUuid::createUuid()),
        m_runtimeId(QnUuid::createUuid()),
        m_videoWallInstanceGuid(QnUuid::createUuid()),
        m_videoWallControlSession(QnUuid::createUuid())
    {
        const auto dbFileArg =
            lm("--dbFile=%1/db.sqlite").args(testDataDir()).toStdString();
        m_process.addArg(dbFileArg.c_str());
    }

    ~ClientRegistration()
    {
        if (m_httpClient)
            m_httpClient->pleaseStopSync();
        if (m_websocket)
            m_websocket->pleaseStopSync();
    }

protected:
    void givenRunningPeer()
    {
        m_process.startAndWaitUntilStarted();
    }

    void whenClientConnects()
    {
        using namespace nx::network;

        QUrlQuery query;
        query.addQueryItem("peerType", "PT_MobileClient");
        query.addQueryItem("guid", m_clientId.toSimpleString());
        query.addQueryItem("runtime-guid", m_runtimeId.toSimpleString());
        query.addQueryItem("videoWallInstanceGuid", m_videoWallInstanceGuid.toSimpleString());
        query.addQueryItem("videoWallControlSession", m_videoWallControlSession.toSimpleString());

        http::HttpHeaders httpHeaders;
        websocket::addClientHeaders(
            &httpHeaders,
            websocket::kWebsocketProtocolName);

        m_httpClient = std::make_unique<http::AsyncClient>();
        m_httpClient->setResponseReadTimeout(kNoTimeout);
        m_httpClient->setAdditionalHeaders(httpHeaders);
        m_httpClient->setUserCredentials(
            http::Credentials("admin", http::PasswordAuthToken("admin")));
        const auto url = url::Builder()
            .setScheme(http::kUrlSchemeName)
            .setEndpoint(m_process.moduleInstance()->endpoint())
            .setPath(nx::p2p::ConnectionBase::kWebsocketUrlPath)
            .setQuery(query.toString()).toUrl();

        m_httpClient->doUpgrade(
            url,
            http::Method::get,
            websocket::kWebsocketProtocolName,
            std::bind(&ClientRegistration::saveClientConnectResult, this));
    }

    void whenClientConnected()
    {
        whenClientConnects();
        thenClientConnectSucceeded();
    }

    void thenClientConnectSucceeded()
    {
        ASSERT_EQ(
            nx::network::http::StatusCode::switchingProtocols,
            m_clientConnectResults.pop());
    }

    void waitUntilPeerRegistersClient()
    {
        auto runtimeInfoManager =
            m_process.moduleInstance()->commonModule()->runtimeInfoManager();

        for (;;)
        {
            if (runtimeInfoManager->hasItem(m_clientId))
            {
                const auto item = runtimeInfoManager->item(m_clientId);
                ASSERT_EQ(m_clientId, item.uuid);
                ASSERT_EQ(m_runtimeId, item.data.peer.instanceId);
                ASSERT_EQ(m_videoWallInstanceGuid, item.data.videoWallInstanceGuid);
                ASSERT_EQ(m_videoWallControlSession, item.data.videoWallControlSession);
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

private:
    nx::utils::test::ModuleLauncher<Appserver2Process> m_process;
    QnUuid m_clientId;
    QnUuid m_runtimeId;
    QnUuid m_videoWallInstanceGuid;
    QnUuid m_videoWallControlSession;
    std::unique_ptr<nx::network::http::AsyncClient> m_httpClient;
    nx::utils::SyncQueue<int /*status code*/> m_clientConnectResults;
    std::unique_ptr<nx::network::websocket::WebSocket> m_websocket;

    void saveClientConnectResult()
    {
        m_clientConnectResults.push(
            m_httpClient->response()->statusLine.statusCode);

        if (m_httpClient->response()->statusLine.statusCode ==
            nx::network::http::StatusCode::switchingProtocols)
        {
            m_websocket = std::make_unique<nx::network::websocket::WebSocket>(
                m_httpClient->takeSocket());
            m_websocket->start();
        }
    }
};

TEST_F(ClientRegistration, client_runtime_info_is_added_by_server)
{
    givenRunningPeer();
    whenClientConnected();
    waitUntilPeerRegistersClient();
}

} // namespace ec2::test
