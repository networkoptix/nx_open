#include "discovery_test_setup.h"

#include <memory>

#include <nx/fusion/model_functions.h>
#include <nx/network/http/server/handler/http_server_handler_create_tunnel.h>
#include <nx/network/websocket/websocket_handshake.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/std/cpp14.h>

namespace nx {
namespace cloud {
namespace discovery {
namespace test {

static const char* const kTestPath = "/create_websocket";

const char* const PeerInformation::kTypeName = "test_peer";

PeerInformation::PeerInformation():
    BasicInstanceInformation(kTypeName)
{
}

bool operator==(const PeerInformation& left, const PeerInformation& right)
{
    if (!(static_cast<const BasicInstanceInformation&>(left) ==
          static_cast<const BasicInstanceInformation&>(right)))
    {
        return false;
    }

    return left.opaque == right.opaque;
}

//-------------------------------------------------------------------------------------------------

void DiscoveryTestSetup::SetUp()
{
    m_httpServer = std::make_unique<nx::network::http::TestHttpServer>();
    registerWebSocketAcceptHandlerAt(kTestPath);

    ASSERT_TRUE(m_httpServer->bindAndListen());
}

void DiscoveryTestSetup::onWebSocketAccepted(
    std::unique_ptr<nx::network::WebSocket> connection)
{
    connection->start();
    m_acceptedConnections.push(std::move(connection));
}

nx::utils::Url DiscoveryTestSetup::getUrl() const
{
    return nx::network::url::Builder()
        .setScheme(nx::network::http::kUrlSchemeName)
        .setEndpoint(m_httpServer->serverAddress())
        .setPath(kTestPath);
}

nx::network::http::TestHttpServer& DiscoveryTestSetup::httpServer()
{
    return *m_httpServer;
}

void DiscoveryTestSetup::stopHttpServer()
{
    m_httpServer.reset();
}

void DiscoveryTestSetup::registerWebSocketAcceptHandlerAt(const nx::network::http::StringType path)
{
    using namespace std::placeholders;

    m_httpServer->registerRequestProcessor<nx::network::http::server::handler::CreateTunnelHandler>(
        path,
        [this]()
        {
            return std::make_unique<nx::network::http::server::handler::CreateTunnelHandler>(
                nx::network::websocket::kWebsocketProtocolName,
                std::bind(&DiscoveryTestSetup::onUpgradedConnectionAccepted, this, _1));
        });
}

void DiscoveryTestSetup::onUpgradedConnectionAccepted(
    std::unique_ptr<nx::network::AbstractStreamSocket> connection)
{
    auto webSocket = std::make_unique<nx::network::WebSocket>(
        std::move(connection), nx::network::websocket::Role::server,
        nx::network::websocket::FrameType::binary,
        nx::network::websocket::CompressionType::perMessageDeflate);
    onWebSocketAccepted(std::move(webSocket));
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (PeerInformation),
    (json),
    _Test_Fields)

} // namespace test
} // namespace discovery
} // namespace cloud
} // namespace nx
