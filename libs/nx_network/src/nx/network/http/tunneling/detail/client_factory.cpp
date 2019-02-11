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

static constexpr auto kEveryTunnelTypeWeight = 1;
/** With every tunnel failure, 20 seconds are added to that tunnel type retry period. */
static constexpr auto kTunnelTypeSinkDepth = 20;
/** Effectively, maximum tunnel type retry period. */
static constexpr auto kMaxTunnelTypeSinkDepth = 600;
// Depth units per second.
static constexpr auto kTunnelTypeRestoreSpeed = 1;

ClientFactory::ClientFactory():
    base_type([this](auto&&... args) { return defaultFactoryFunction(std::move(args)...); })
{
    registerClientType<ConnectionUpgradeTunnelClient>();
    registerClientType<GetPostTunnelClient>();
    registerClientType<ExperimentalTunnelClient>();
    registerClientType<SslTunnelClient>();
}

int ClientFactory::topTunnelTypeId(const std::string& tag) const
{
    QnMutexLocker lock(&m_mutex);

    auto it = m_tagToTunnelTypeSelector.find(tag);
    return it != m_tagToTunnelTypeSelector.end()
        ? it->second.topItem()
        : -1;
}

void ClientFactory::clear()
{
    QnMutexLocker lock(&m_mutex);

    m_clientTypes.clear();
    m_tagToTunnelTypeSelector.clear();
}

ClientFactory& ClientFactory::instance()
{
    static ClientFactory staticInstance;
    return staticInstance;
}

std::unique_ptr<BaseTunnelClient> ClientFactory::defaultFactoryFunction(
    const std::string& tag,
    const utils::Url& baseUrl)
{
    QnMutexLocker lock(&m_mutex);

    auto it = m_tagToTunnelTypeSelector.find(tag);
    if (it == m_tagToTunnelTypeSelector.end())
        it = m_tagToTunnelTypeSelector.emplace(tag, buildTunnelTypeSelector()).first;

    const auto tunnelTypeId = it->second.topItem();

    auto clientTypeIter = m_clientTypes.find(tunnelTypeId);
    NX_CRITICAL(clientTypeIter != m_clientTypes.end());

    return clientTypeIter->second.factoryFunction(
        baseUrl,
        [this, tunnelTypeId, tag](bool success)
        {
            processClientFeedback(tunnelTypeId, tag, success);
        });
}

void ClientFactory::registerClientType(
    InternalFactoryFunction factoryFunction)
{
    QnMutexLocker lock(&m_mutex);

    m_clientTypes.emplace(
        ++m_prevUsedTypeId,
        ClientTypeContext{std::move(factoryFunction)});

    // Using same weight for each tunnel since m_tunnelTypeSelector selects
    // first-added item from items on the same depth.

    for (auto& tagAndTunnelTypeSelector: m_tagToTunnelTypeSelector)
        tagAndTunnelTypeSelector.second.add(m_prevUsedTypeId, kEveryTunnelTypeWeight);
}

ClientFactory::TunnelTypeSelector ClientFactory::buildTunnelTypeSelector()
{
    TunnelTypeSelector tunnelTypeSelector(
        std::chrono::seconds(1) / kTunnelTypeRestoreSpeed);
    tunnelTypeSelector.setMaxSinkDepth(kMaxTunnelTypeSinkDepth);

    for (const auto& idAndContext: m_clientTypes)
        tunnelTypeSelector.add(idAndContext.first, kEveryTunnelTypeWeight);

    return tunnelTypeSelector;
}

void ClientFactory::processClientFeedback(
    int tunnelTypeId, const std::string& tag, bool success)
{
    if (success)
        return;

    // Recording only failures.

    QnMutexLocker lock(&m_mutex);

    auto it = m_tagToTunnelTypeSelector.find(tag);
    NX_ASSERT(it != m_tagToTunnelTypeSelector.end());
    if (it == m_tagToTunnelTypeSelector.end())
        return;

    it->second.sinkItem(tunnelTypeId, kTunnelTypeSinkDepth);
}

} // namespace nx::network::http::tunneling::detail
