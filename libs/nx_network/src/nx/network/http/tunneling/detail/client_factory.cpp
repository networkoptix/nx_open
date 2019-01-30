#include "client_factory.h"

#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/cloud/cloud_connect_settings.h>
#include <nx/network/socket_global.h>
#include <nx/utils/std/algorithm.h>

#include "get_post_tunnel_client.h"
#include "connection_upgrade_tunnel_client.h"
#include "experimental_tunnel_client.h"
#include "ssl_tunnel_client.h"

namespace nx::network::http::tunneling::detail {

/**
 * Settings for automatic tunnel type restoration.
 */

constexpr auto kEveryTunnelTypeWeight = 1;
/** With every tunnel failure, 20 seconds are added to that tunnel type retry period. */
constexpr auto kTunnelTypeSinkDepth = 20;
/** Effectively, maximum tunnel type retry period. */
constexpr auto kMaxTunnelTypeSinkDepth = 600;
// Depth units per second.
constexpr auto kTunnelTypeRestoreSpeed = 1;

ClientFactory::ClientFactory():
    base_type([this](auto&&... args) { return defaultFactoryFunction(std::move(args)...); }),
    m_tunnelTypeSelector(std::chrono::seconds(1) / kTunnelTypeRestoreSpeed)
{
    m_tunnelTypeSelector.setMaxSinkDepth(kMaxTunnelTypeSinkDepth);

    registerClientType<ConnectionUpgradeTunnelClient>();
    registerClientType<GetPostTunnelClient>();
    registerClientType<ExperimentalTunnelClient>();
    registerClientType<SslTunnelClient>();
}

void ClientFactory::clear()
{
    QnMutexLocker lock(&m_mutex);

    m_clientTypes.clear();
    m_tunnelTypeSelector.clear();
}

ClientFactory& ClientFactory::instance()
{
    static ClientFactory staticInstance;
    return staticInstance;
}

std::unique_ptr<BaseTunnelClient> ClientFactory::defaultFactoryFunction(
    const utils::Url& baseUrl)
{
    using namespace std::placeholders;

    QnMutexLocker lock(&m_mutex);

    const auto tunnelTypeId = m_tunnelTypeSelector.topItem();

    auto clientTypeIter = m_clientTypes.find(tunnelTypeId);
    NX_CRITICAL(clientTypeIter != m_clientTypes.end());

    return clientTypeIter->second.factoryFunction(
        baseUrl,
        [this, tunnelTypeId](bool success)
        {
            processClientFeedback(tunnelTypeId, success);
        });
}

void ClientFactory::registerClientType(
    InternalFactoryFunction factoryFunction)
{
    QnMutexLocker lock(&m_mutex);

    m_clientTypes.emplace(
        ++m_prevUsedTypeId,
        ClientTypeContext{ std::move(factoryFunction) });

    // Using same weight for each tunnel since m_tunnelTypeSelector selects
    // first-added item from items on the same depth.
    NX_CRITICAL(m_tunnelTypeSelector.add(m_prevUsedTypeId, kEveryTunnelTypeWeight));
}

void ClientFactory::processClientFeedback(int tunnelTypeId, bool success)
{
    QnMutexLocker lock(&m_mutex);

    if (!success)
        m_tunnelTypeSelector.sinkItem(tunnelTypeId, kTunnelTypeSinkDepth);
}

} // namespace nx::network::http::tunneling::detail
