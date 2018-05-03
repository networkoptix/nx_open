
#include "system_description_aggregator.h"

#include <nx/utils/log/assert.h>

namespace {

QnBaseSystemDescription::ServersList subtractLists(
    const QnBaseSystemDescription::ServersList& first,
    const QnBaseSystemDescription::ServersList& second)
{
    QnBaseSystemDescription::ServersList result;
    std::copy_if(first.begin(), first.end(), std::back_inserter(result),
        [&second](const QnModuleInformation& first)
        {
            const auto secondIt = std::find_if(second.begin(), second.end(),
                                                [first](const QnModuleInformation& second)
            {
                return (first.id == second.id);
            });

            return (secondIt == second.end()); // add to result if not found in second
        });
    return result;
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
    for (const auto systemDescription : m_systems)
    {
        if (systemDescription->id() == systemId)
            return true;
    }
    return false;
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

    const int lastPriority = (m_systems.isEmpty() ? 0 : m_systems.firstKey());
    m_systems.insert(priority, system);

    /**
     * We gather all servers for aggregated systems - for example, we may have
     * offline cloud system and same discovered local one
     */
    connect(system, &QnBaseSystemDescription::serverAdded,
        this, &QnSystemDescriptionAggregator::updateServers);
    connect(system, &QnBaseSystemDescription::serverRemoved,
        this, &QnSystemDescriptionAggregator::updateServers);

    connect(system, &QnBaseSystemDescription::safeModeStateChanged,
        this, &QnSystemDescriptionAggregator::safeModeStateChanged);
    connect(system, &QnBaseSystemDescription::newSystemStateChanged,
        this, &QnSystemDescriptionAggregator::newSystemStateChanged);

    connect(system, &QnBaseSystemDescription::serverChanged,
        this, &QnSystemDescriptionAggregator::handleServerChanged);
    connect(system, &QnBaseSystemDescription::systemNameChanged, this,
        [this, system]() { onSystemNameChanged(system); });

    connect(system, &QnBaseSystemDescription::runningStateChanged,
        this, &QnBaseSystemDescription::runningStateChanged);
    connect(system, &QnBaseSystemDescription::reachableStateChanged,
        this, &QnBaseSystemDescription::reachableStateChanged);
    connect(system, &QnBaseSystemDescription::connectableStateChanged,
        this, &QnBaseSystemDescription::connectableStateChanged);

    updateServers();
    emitSystemChanged();
}

void QnSystemDescriptionAggregator::emitSystemChanged()
{
    emit isCloudSystemChanged();
    emit ownerChanged();
    emit systemNameChanged();
    emit runningStateChanged();
    emit reachableStateChanged();
    emit connectableStateChanged();
    emit safeModeStateChanged();
    emit newSystemStateChanged();
}

void QnSystemDescriptionAggregator::handleServerChanged(const QnUuid& serverId,
    QnServerFields fields)
{
    const auto it = std::find_if(m_servers.begin(), m_servers.end(),
        [serverId](const QnModuleInformation& info) { return (serverId == info.id); });

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
    return (isEmptyAggregator() ? false : m_systems.first()->isCloudSystem());
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

bool QnSystemDescriptionAggregator::safeMode() const
{
    if (isEmptyAggregator())
        return false;

    return std::any_of(m_systems.begin(), m_systems.end(),
        [](const QnSystemDescriptionPtr& system) { return system->safeMode(); });
}

bool QnSystemDescriptionAggregator::isRunning() const
{
    if (isEmptyAggregator())
        return false;

    return std::any_of(m_systems.begin(), m_systems.end(),
        [](const QnSystemDescriptionPtr& system)
        {
            return system->isRunning();
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

bool QnSystemDescriptionAggregator::isConnectable() const
{
    if (isEmptyAggregator())
        return false;

    return std::any_of(m_systems.begin(), m_systems.end(),
        [](const QnSystemDescriptionPtr& system)
        {
            return system->isConnectable();
        });
}

void QnSystemDescriptionAggregator::updateServers()
{
    const auto newServers = gatherServers();
    const auto toRemove = subtractLists(m_servers, newServers);
    const auto toAdd = subtractLists(newServers, m_servers);

    m_servers = newServers;
    for (const auto& server : toRemove)
        emit serverRemoved(server.id);

    for (const auto& server : toAdd)
        emit serverAdded(server.id);

    /**
     * Updates server host in case we remove cloud system but have accesible local one.
     * See VMS-5884.
     */
    for (const auto& server: subtractLists(m_servers, toAdd))
        emit serverChanged(server.id, QnServerField::Host);
}

QnBaseSystemDescription::ServersList QnSystemDescriptionAggregator::gatherServers() const
{
    ServersList result;
    for (const auto systemDescription : m_systems)
    {
        const auto toAddList = subtractLists(systemDescription->servers(), result);
        std::copy(toAddList.begin(), toAddList.end(), std::back_inserter(result));
    }
    return result;
}

bool QnSystemDescriptionAggregator::containsServer(const QnUuid& serverId) const
{
    for (const auto systemDescription : m_systems)
    {
        if (systemDescription->containsServer(serverId))
            return true;
    }
    return false;
}

QnModuleInformation QnSystemDescriptionAggregator::getServer(const QnUuid& serverId) const
{
    for (const auto systemDescription : m_systems)
    {
        if (systemDescription->containsServer(serverId))
            return systemDescription->getServer(serverId);
    }
    return QnModuleInformation();
}

nx::utils::Url QnSystemDescriptionAggregator::getServerHost(const QnUuid& serverId) const
{
    for (const auto systemDescription : m_systems)
    {
        if (systemDescription->containsServer(serverId))
            return systemDescription->getServerHost(serverId);
    }

    return QString();
}

qint64 QnSystemDescriptionAggregator::getServerLastUpdatedMs(const QnUuid& serverId) const
{
    qint64 result = 0;

    for (const auto systemDescription : m_systems)
    {
        if (systemDescription->containsServer(serverId))
            result = std::max<qint64>(result, systemDescription->getServerLastUpdatedMs(serverId));
    }

    return result;
}
