#include <list>

#include <gtest/gtest.h>

#include <nx/network/http/rest/http_rest_client.h>

#include <discovery/discovery_client.h>

#include "discovery_test_setup.h"

namespace nx {
namespace cloud {
namespace discovery {
namespace test {

class ModuleRegistrar:
    public DiscoveryTestSetup
{
    using base_type = DiscoveryTestSetup;

public:
    ModuleRegistrar():
        m_moduleId(QnUuid::createUuid().toSimpleString().toStdString())
    {
    }

    ~ModuleRegistrar()
    {
        stopHttpServer();

        if (m_registrar)
            m_registrar->pleaseStopSync();

        m_aioThreadBinder.executeInAioThreadSync(
            [this]() { m_webSockets.clear(); });
        m_aioThreadBinder.pleaseStopSync();
    }

protected:
    virtual void SetUp() override
    {
        base_type::SetUp();

        registerWebSocketAcceptHandlerAt(
            nx_http::rest::substituteParameters(
                http::kModuleKeepAliveConnectionPath, { m_moduleId.c_str() }).c_str());
    }

    virtual void onWebSocketAccepted(
        std::unique_ptr<nx::network::WebSocket> connection) override
    {
        m_aioThreadBinder.post(
            [this, connection = std::move(connection)]() mutable
            {
                addWebSocket(std::move(connection));
            });
    }

    void givenNotConnectedClient()
    {
        auto baseUrl = getUrl();
        baseUrl.setPath("/");
        m_registrar = std::make_unique<discovery::ModuleRegistrar<PeerInformation>>(
            baseUrl,
            m_moduleId);
    }

    void whenAddMultipleModuleInformation()
    {
        for (int i = 0; i < 3; ++i)
            whenAddRandomModuleInformation();
    }

    void whenAddRandomModuleInformation()
    {
        PeerInformation peerInformation;
        peerInformation.id = m_moduleId;
        peerInformation.opaque = nx::utils::generateRandomName(7).toStdString();
        m_prevPeerInformation = peerInformation;
        m_registrar->setInstanceInformation(peerInformation);
    }

    void whenConnectClient()
    {
        m_registrar->start();
    }

    void thenOnlyLastModuleInformationHasBeenReported()
    {
        auto receivedModuleInformation = popReportedModuleInformation();
        ASSERT_EQ(m_prevPeerInformation, receivedModuleInformation);
    }

private:
    struct WebSocketContext
    {
        std::unique_ptr<nx::network::WebSocket> webSocket;
        nx::Buffer readBuffer;
    };

    const std::string m_moduleId;
    std::unique_ptr<discovery::ModuleRegistrar<PeerInformation>> m_registrar;
    PeerInformation m_prevPeerInformation;
    std::list<std::unique_ptr<WebSocketContext>> m_webSockets;
    nx::network::aio::BasicPollable m_aioThreadBinder;
    nx::utils::SyncQueue<PeerInformation> m_acceptedModuleInformation;

    void addWebSocket(std::unique_ptr<nx::network::WebSocket> connection)
    {
        auto context = std::make_unique<WebSocketContext>();
        connection->bindToAioThread(m_aioThreadBinder.getAioThread());
        context->webSocket = std::move(connection);
        context->webSocket->start();

        m_webSockets.push_back(std::move(context));
        scheduleAsyncRead(m_webSockets.back().get());
    }

    PeerInformation popReportedModuleInformation()
    {
        return m_acceptedModuleInformation.pop();
    }

    void scheduleAsyncRead(WebSocketContext* webSocketContext)
    {
        using namespace std::placeholders;

        webSocketContext->readBuffer.clear();
        webSocketContext->readBuffer.reserve(16 * 1024);
        webSocketContext->webSocket->readSomeAsync(
            &webSocketContext->readBuffer,
            std::bind(&ModuleRegistrar::onWebSocketRead, this, webSocketContext, _1, _2));
    }

    void onWebSocketRead(
        WebSocketContext* webSocketContext,
        SystemError::ErrorCode systemErrorCode,
        std::size_t bytesRead)
    {
        if ((systemErrorCode == SystemError::noError && bytesRead == 0) ||
            nx::network::socketCannotRecoverFromError(systemErrorCode))
        {
            webSocketContext->webSocket.reset();
            for (auto it = m_webSockets.begin(); it != m_webSockets.end(); ++it)
            {
                if (it->get() == webSocketContext)
                {
                    m_webSockets.erase(it);
                    break;
                }
            }
            return;
        }

        if (systemErrorCode == SystemError::noError)
        {
            m_acceptedModuleInformation.push(
                QJson::deserialized<PeerInformation>(webSocketContext->readBuffer));
        }

        scheduleAsyncRead(webSocketContext);
    }
};

TEST_F(ModuleRegistrar, unsent_module_information_skipped_in_preference_to_new_one)
{
    givenNotConnectedClient();
    whenAddMultipleModuleInformation();
    whenConnectClient();
    thenOnlyLastModuleInformationHasBeenReported();
}

} // namespace test
} // namespace discovery
} // namespace cloud
} // namespace nx
