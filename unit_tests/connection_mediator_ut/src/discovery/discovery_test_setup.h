#pragma once

#include <string>

#include <gtest/gtest.h>

#include <QtCore/QUrl>

#include <nx/network/http/test_http_server.h>
#include <nx/network/websocket/websocket.h>
#include <nx/utils/thread/sync_queue.h>

#include <discovery/discovery_client.h>

namespace nx {
namespace cloud {
namespace discovery {
namespace test {

struct PeerInformation:
    BasicInstanceInformation
{
    static const char* const kTypeName;

    std::string opaque;

    PeerInformation();
};

#define PeerInformation_Test_Fields BasicInstanceInformation_Fields (opaque)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (PeerInformation),
    (json))

bool operator==(const PeerInformation& left, const PeerInformation& right);

class DiscoveryTestSetup:
    public ::testing::Test
{
protected:
    // TODO: #ak Better move it to private section.
    nx::utils::SyncQueue<std::unique_ptr<nx::network::WebSocket>> m_acceptedConnections;

    virtual void SetUp() override;

    virtual void onWebSocketAccepted(std::unique_ptr<nx::network::WebSocket> connection);

    utils::Url getUrl() const;
    TestHttpServer& httpServer();
    void stopHttpServer();

    void registerWebSocketAcceptHandlerAt(const nx::network::http::StringType path);

private:
    std::unique_ptr<TestHttpServer> m_httpServer;

    void onUpgradedConnectionAccepted(std::unique_ptr<AbstractStreamSocket> connection);
};

} // namespace test
} // namespace discovery
} // namespace cloud
} // namespace nx
