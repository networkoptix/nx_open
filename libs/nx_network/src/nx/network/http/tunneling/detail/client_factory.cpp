// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "client_factory.h"

#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/cloud/cloud_connect_settings.h>
#include <nx/network/socket_global.h>
#include <nx/utils/log/log.h>
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
/** With every tunnel failure, these seconds are added to that tunnel type retry period. */
static constexpr auto kTunnelTypeSinkDepth = 100;
/** A tunnel may be ascended among multiple working tunnel types to point out a preferred one. */
static constexpr auto kTunnelTypeMaxAscendLevel = 10;
static constexpr auto kTunnelTypeAscendStep = 1;
/** Effectively, maximum tunnel type retry period. */
static constexpr auto kMaxTunnelTypeSinkDepth = 600;
/** Tunnel type emerge speed (depth units per second). */
static constexpr auto kTunnelTypeRestoreSpeed = 1;

ClientFactory::ClientFactory():
    base_type([this](auto&&... args) { return defaultFactoryFunction(std::move(args)...); })
{
    reset();
}

std::vector<int> ClientFactory::topTunnelTypeIds(const std::string& tag) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    auto it = m_tagToTunnelTypeSelector.find(tag);
    if (it == m_tagToTunnelTypeSelector.end())
        it = m_tagToTunnelTypeSelector.emplace(tag, buildTunnelTypeSelector()).first;

    return it->second.topItemRange();
}

void ClientFactory::clear()
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    m_clientTypes.clear();
    m_tagToTunnelTypeSelector.clear();
}

void ClientFactory::reset()
{
    clear();

    registerClientType<GetPostTunnelClient>(0);
    registerClientType<ConnectionUpgradeTunnelClient>(0);
    // registerClientType<ExperimentalTunnelClient>();
    registerClientType<SslTunnelClient>(50);
}

void ClientFactory::removeForcedClientType()
{
    setForcedClientFactory(nullptr);
}

ClientFactory& ClientFactory::instance()
{
    static ClientFactory staticInstance;
    return staticInstance;
}

std::vector<std::unique_ptr<BaseTunnelClient>> ClientFactory::defaultFactoryFunction(
    const std::string& tag,
    const nx::utils::Url& baseUrl,
    std::optional<int> forcedTunnelType)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    if (m_forcedClientFactory)
    {
        // No need to handle the feedback here since the client type has been forced.
        std::vector<std::unique_ptr<BaseTunnelClient>> result;
        result.push_back(m_forcedClientFactory(baseUrl, [](bool /*success*/) {}));
        return result;
    }

    auto it = m_tagToTunnelTypeSelector.find(tag);
    if (it == m_tagToTunnelTypeSelector.end())
        it = m_tagToTunnelTypeSelector.emplace(tag, buildTunnelTypeSelector()).first;

    const auto tunnelTypeIds = forcedTunnelType
        ? std::vector<int>{*forcedTunnelType}
        : it->second.topItemRange();

    std::vector<std::unique_ptr<BaseTunnelClient>> result;
    for (const auto tunnelTypeId: tunnelTypeIds)
    {
        auto clientTypeIter = m_clientTypes.find(tunnelTypeId);
        NX_CRITICAL(clientTypeIter != m_clientTypes.end());

        result.push_back(
            clientTypeIter->second.factoryFunction(
                baseUrl,
                [this, tunnelTypeId, tag](bool success)
                {
                    processClientFeedback(tunnelTypeId, tag, success);
                }));
    }

    return result;
}

int ClientFactory::registerClientType(
    InternalFactoryFunction factoryFunction,
    int initialPriority)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    m_clientTypes.emplace(
        ++m_prevUsedTypeId,
        ClientTypeContext{std::move(factoryFunction), initialPriority});

    // Using same weight for each tunnel since m_tunnelTypeSelector selects
    // first-added item from items on the same depth.

    for (auto& tagAndTunnelTypeSelector: m_tagToTunnelTypeSelector)
    {
        tagAndTunnelTypeSelector.second.add(
            m_prevUsedTypeId, kEveryTunnelTypeWeight, initialPriority);
    }

    return m_prevUsedTypeId;
}

ClientFactory::TunnelTypeSelector ClientFactory::buildTunnelTypeSelector() const
{
    TunnelTypeSelector tunnelTypeSelector(
        std::chrono::seconds(1) / kTunnelTypeRestoreSpeed);
    tunnelTypeSelector.setMaxSinkDepth(kMaxTunnelTypeSinkDepth);
    tunnelTypeSelector.setMaxAscendLevel(kTunnelTypeMaxAscendLevel);

    for (const auto& idAndContext: m_clientTypes)
    {
        tunnelTypeSelector.add(
            idAndContext.first,
            kEveryTunnelTypeWeight,
            idAndContext.second.initialPriority);
    }

    return tunnelTypeSelector;
}

void ClientFactory::processClientFeedback(
    int tunnelTypeId, const std::string& tag, bool success)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    auto it = m_tagToTunnelTypeSelector.find(tag);
    if (it == m_tagToTunnelTypeSelector.end())
    {
        NX_DEBUG(this, "No tunnel");
        return;
    }

    if (success)
    {
        NX_VERBOSE(this, "Got tunnel type %1 success for tag %2", tunnelTypeId, tag);
        it->second.ascendItem(tunnelTypeId, kTunnelTypeAscendStep);
    }
    else
    {
        it->second.sinkItem(tunnelTypeId, kTunnelTypeSinkDepth);

        NX_DEBUG(this, "Tunnel type %1 failure. Tag %2. Tunnel type set to %3",
            tunnelTypeId, tag, it->second.topItem());
    }
}

void ClientFactory::setForcedClientFactory(InternalFactoryFunction func)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_forcedClientFactory = std::move(func);
}

} // namespace nx::network::http::tunneling::detail
