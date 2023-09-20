// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_description_aggregator.h"

#include <nx/network/address_resolver.h>
#include <nx/network/socket_global.h>
#include <nx/utils/log/assert.h>

namespace {

QnBaseSystemDescription::ServersList subtractLists(
    QnBaseSystemDescription::ServersList minuend,
    const QnBaseSystemDescription::ServersList& subtrahend)
{
    const auto newEnd = std::remove_if(minuend.begin(), minuend.end(),
        [subtrahend](const nx::vms::api::ModuleInformationWithAddresses& current)
        {
            return std::any_of(subtrahend.begin(), subtrahend.end(),
                [currentId = current.id](const nx::vms::api::ModuleInformationWithAddresses& other)
                {
                    return other.id == currentId;
                });
        });

    minuend.erase(newEnd, minuend.end());
    return minuend;
};

bool isSameSystem(
    const QnBaseSystemDescription& first,
    const QnBaseSystemDescription& second)
{
    typedef QSet<QnUuid> ServerIdsSet;
    static const auto extractServerIds =
        [](const QnBaseSystemDescription::ServersList& servers) -> ServerIdsSet
        {
            ServerIdsSet result;
            for (const auto& server: servers)
                result.insert(server.id);
            return result;
        };

    // Systems are the same if they have same set of servers
    return extractServerIds(first.servers()) == extractServerIds(second.servers());
}

bool isCloudHost(const QString& host)
{
    return nx::network::SocketGlobals::addressResolver().isCloudHostname(host);
}

}   // namespace

QnSystemDescriptionAggregator::QnSystemDescriptionAggregator(int priority,
    const QnSystemDescriptionPtr& systemDescription)
    :
    m_systems()
{
    mergeSystem(priority, systemDescription);
}

bool QnSystemDescriptionAggregator::isEmptyAggregator() const
{
    const bool emptySystemsList = m_systems.empty();
    NX_ASSERT(!emptySystemsList, "Empty systems aggregator detected");
    return emptySystemsList;
}

bool QnSystemDescriptionAggregator::isAggregator() const
{
    return (m_systems.size() > 1);
}

bool QnSystemDescriptionAggregator::containsSystem(const QString& systemId) const
{
    for (const auto& systemDescription: m_systems)
    {
        if (systemDescription->id() == systemId)
            return true;
    }
    return false;
}

bool QnSystemDescriptionAggregator::containsSystem(int priority) const
{
    return m_systems.contains(priority);
}

void QnSystemDescriptionAggregator::mergeSystem(int priority,
    const QnSystemDescriptionPtr& system)
{
    NX_ASSERT(system, "System is empty");
    if (!system)
        return;

    const bool exists = m_systems.contains(priority);
    if (exists)
        return;

    m_systems.insert(priority, system);

    /**
     * We gather all servers for aggregated systems - for example, we may have
     * offline cloud system and same discovered local one
     */
    connect(system.get(), &QnBaseSystemDescription::serverAdded,
        this, &QnSystemDescriptionAggregator::updateServers);
    connect(system.get(), &QnBaseSystemDescription::serverRemoved,
        this, &QnSystemDescriptionAggregator::updateServers);

    connect(system.get(), &QnBaseSystemDescription::newSystemStateChanged,
        this, &QnSystemDescriptionAggregator::newSystemStateChanged);

    connect(system.get(), &QnBaseSystemDescription::serverChanged,
        this, &QnSystemDescriptionAggregator::handleServerChanged);
    connect(system.get(), &QnBaseSystemDescription::systemNameChanged, this,
        [this, system]() { onSystemNameChanged(system); });
    connect(system.get(), &QnBaseSystemDescription::versionChanged, this,
        &QnSystemDescriptionAggregator::versionChanged);

    connect(system.get(), &QnBaseSystemDescription::onlineStateChanged,
        this, &QnSystemDescriptionAggregator::onOnlineStateChanged);
    connect(system.get(), &QnBaseSystemDescription::reachableStateChanged,
        this, &QnBaseSystemDescription::reachableStateChanged);
    connect(system.get(), &QnBaseSystemDescription::oauthSupportedChanged,
        this, &QnBaseSystemDescription::oauthSupportedChanged);
    connect(system.get(), &QnBaseSystemDescription::system2faEnabledChanged,
        this, &QnBaseSystemDescription::system2faEnabledChanged);
    connect(system.get(), &QnBaseSystemDescription::saasStateChanged, this,
        [this]
        {
            if (m_systems.first() == sender())
                emit saasStateChanged();
        });

    updateServers();
    emitSystemChanged();
}

void QnSystemDescriptionAggregator::emitSystemChanged()
{
    emit isCloudSystemChanged();
    emit ownerChanged();
    emit systemNameChanged();
    emit onlineStateChanged();
    emit reachableStateChanged();
    emit newSystemStateChanged();
    emit system2faEnabledChanged();
    emit versionChanged();
    emit saasStateChanged();
}

void QnSystemDescriptionAggregator::handleServerChanged(const QnUuid& serverId,
    QnServerFields fields)
{
    const auto it = std::find_if(m_servers.begin(), m_servers.end(),
        [serverId](const nx::vms::api::ModuleInformationWithAddresses& info) { return (serverId == info.id); });

    if (it != m_servers.end())
        emit serverChanged(serverId, fields);
}

void QnSystemDescriptionAggregator::onSystemNameChanged(const QnSystemDescriptionPtr& system)
{
    if (isEmptyAggregator() || !system)
        return;

    if (isSameSystem(*system, *m_systems.first()))
        emit systemNameChanged();
}

void QnSystemDescriptionAggregator::onOnlineStateChanged()
{
    if (isEmptyAggregator())
        return;

    // Offline cloud system may be listed as a local one. Send signal to refresh it's state.
    if (m_systems.first()->isCloudSystem())
    {
        emit isCloudSystemChanged();
        for (const auto& server: m_systems.first()->servers())
            emit serverChanged(server.id, QnServerField::Host);
    }

    emit onlineStateChanged();
}

void QnSystemDescriptionAggregator::removeSystem(int priority)
{
    const bool exist = m_systems.contains(priority);
    NX_ASSERT(exist, "System does not exist");
    if (!exist)
        return;

    const auto oldServers = servers();
    const auto system = m_systems.value(priority);

    disconnect(system.data(), nullptr, this, nullptr);
    m_systems.remove(priority);

    updateServers();

    if (!m_systems.isEmpty())
        emitSystemChanged();
}

QString QnSystemDescriptionAggregator::id() const
{
    return (isEmptyAggregator() ? QString() : m_systems.first()->id());
}

QnUuid QnSystemDescriptionAggregator::localId() const
{
    return (isEmptyAggregator() ? QnUuid() : m_systems.first()->localId());
}

QString QnSystemDescriptionAggregator::name() const
{
    return (isEmptyAggregator() ? QString() : m_systems.first()->name());
}

bool QnSystemDescriptionAggregator::isCloudSystem() const
{
    if (isEmptyAggregator() || !m_systems.first()->isCloudSystem())
        return false;

    // Show offline and reachable cloud system as local.
    return m_systems.first()->isOnline() || !isReachable();
}

bool QnSystemDescriptionAggregator::is2FaEnabled() const
{
    if (isEmptyAggregator())
        return false;

    // If the aggregator has cloud system, it would be the first in the m_systems map.
    return m_systems.first()->is2FaEnabled();
}

bool QnSystemDescriptionAggregator::isNewSystem() const
{
    if (isEmptyAggregator())
        return false;

    return std::any_of(m_systems.begin(), m_systems.end(),
        [](const QnSystemDescriptionPtr& system) { return system->isNewSystem(); });
}


QString QnSystemDescriptionAggregator::ownerAccountEmail() const
{
    return (isEmptyAggregator() ? QString() : m_systems.first()->ownerAccountEmail());
}

QString QnSystemDescriptionAggregator::ownerFullName() const
{
    return (isEmptyAggregator() ? QString() : m_systems.first()->ownerFullName());
}

QnBaseSystemDescription::ServersList QnSystemDescriptionAggregator::servers() const
{
    return m_servers;
}

nx::vms::api::SaasState QnSystemDescriptionAggregator::saasState() const
{
    return isEmptyAggregator()
        ? nx::vms::api::SaasState::uninitialized
        : m_systems.first()->saasState();
}

bool QnSystemDescriptionAggregator::isReachableServer(const QnUuid& serverId) const
{
    if (isEmptyAggregator())
        return false;

    return std::any_of(m_systems.begin(), m_systems.end(),
        [serverId](const QnSystemDescriptionPtr& system)
        {
            return system->isReachableServer(serverId);
        });
}

bool QnSystemDescriptionAggregator::isOnline() const
{
    if (isEmptyAggregator())
        return false;

    return std::any_of(m_systems.begin(), m_systems.end(),
        [](const QnSystemDescriptionPtr& system)
        {
            return system->isOnline();
        });
}

bool QnSystemDescriptionAggregator::isReachable() const
{
    if (isEmptyAggregator())
        return false;

    return std::any_of(m_systems.begin(), m_systems.end(),
        [](const QnSystemDescriptionPtr& system)
        {
            return system->isReachable();
        });
}

void QnSystemDescriptionAggregator::updateServers()
{
    const auto newServers = gatherServers();
    const auto toRemove = subtractLists(m_servers, newServers);
    const auto toAdd = subtractLists(newServers, m_servers);

    m_servers = newServers;
    for (const auto& server: toRemove)
        emit serverRemoved(server.id);

    for (const auto& server: toAdd)
        emit serverAdded(server.id);

    /**
     * Updates server host in case we remove cloud system but have accesible local one.
     * See VMS-5884.
     */
    for (const auto& server: subtractLists(newServers, toAdd))
        emit serverChanged(server.id, QnServerField::Host);
}

QnBaseSystemDescription::ServersList QnSystemDescriptionAggregator::gatherServers() const
{
    ServersList result;
    for (const auto& systemDescription: m_systems)
    {
        const auto toAddList = subtractLists(systemDescription->servers(), result);
        std::copy(toAddList.begin(), toAddList.end(), std::back_inserter(result));
    }
    return result;
}

bool QnSystemDescriptionAggregator::containsServer(const QnUuid& serverId) const
{
    for (const auto& systemDescription: m_systems)
    {
        if (systemDescription->containsServer(serverId))
            return true;
    }
    return false;
}

nx::vms::api::ModuleInformationWithAddresses QnSystemDescriptionAggregator::getServer(
    const QnUuid& serverId) const
{
    for (const auto& systemDescription: m_systems)
    {
        if (systemDescription->containsServer(serverId))
            return systemDescription->getServer(serverId);
    }
    return {};
}

nx::utils::Url QnSystemDescriptionAggregator::getServerHost(const QnUuid& serverId) const
{
    for (const auto& systemDescription: m_systems)
    {
        if (!systemDescription->containsServer(serverId))
            continue;

        // Do not return cloud host for offline cloud systems.
        const auto url = systemDescription->getServerHost(serverId);
        if (isCloudHost(url.host()) && !isCloudSystem())
            continue;

        return url;
    }

    return {};
}

QSet<nx::utils::Url> QnSystemDescriptionAggregator::getServerRemoteAddresses(
    const QnUuid& serverId) const
{
    for (const auto& systemDescription: m_systems)
    {
        if (systemDescription->containsServer(serverId))
            return systemDescription->getServerRemoteAddresses(serverId);
    }

    return {};
}

qint64 QnSystemDescriptionAggregator::getServerLastUpdatedMs(const QnUuid& serverId) const
{
    qint64 result = 0;

    for (const auto& systemDescription: m_systems)
    {
        if (systemDescription->containsServer(serverId))
            result = std::max<qint64>(result, systemDescription->getServerLastUpdatedMs(serverId));
    }

    return result;
}

bool QnSystemDescriptionAggregator::hasLocalServer() const
{
    if (isEmptyAggregator())
        return false;

    return std::any_of(m_systems.begin(), m_systems.end(),
        [](const QnSystemDescriptionPtr& system)
        {
            return system->hasLocalServer();
        });
}

QnSystemCompatibility QnSystemDescriptionAggregator::systemCompatibility() const
{
    for (const auto& system: m_systems)
    {
        const auto systemCompatibility = system->systemCompatibility();
        if (systemCompatibility == QnSystemCompatibility::compatible
            || systemCompatibility == QnSystemCompatibility::incompatible)
        {
            return systemCompatibility;
        }
    }

    return QnSystemCompatibility::requireCompatibilityMode;
}

bool QnSystemDescriptionAggregator::isOauthSupported() const
{
    if (isEmptyAggregator())
        return true;

    return std::all_of(m_systems.begin(), m_systems.end(),
        [](const QnSystemDescriptionPtr& system)
        {
            return system->isOauthSupported();
        });
}

nx::utils::SoftwareVersion QnSystemDescriptionAggregator::version() const
{
    if (isEmptyAggregator())
        return {};

    return (*std::max_element(m_systems.cbegin(), m_systems.cend(),
        [](const auto& l, const auto& r)
        {
            return l->version() < r->version();
        }))->version();
}
