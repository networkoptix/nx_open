#include "recent_local_systems_finder.h"

#include <nx/network/address_resolver.h>
#include <nx/network/deprecated/asynchttpclient.h>
#include <nx/network/socket_global.h>

#include <client_core/client_core_settings.h>
#include <client_core/local_connection_data.h>

namespace {

const auto isAcceptablePred =
    [](const QnSystemDescriptionPtr& system, const QString& systemId, const QnUuid& localId)
    {
        // Filters out all systems with local id that exists in discovered systems
        return ((systemId != system->id()) && (localId.toString() != system->id()));
    };

} // namespace

QnRecentLocalSystemsFinder::QnRecentLocalSystemsFinder(QObject* parent) :
    base_type(parent),
    m_filteringSystems(),
    m_finalSystems()
{
    connect(qnClientCoreSettings, &QnClientCoreSettings::valueChanged, this,
        [this](int valueId)
    {
        if (valueId == QnClientCoreSettings::RecentLocalConnections)
            updateSystems();
    });

    updateSystems();
}

QnAbstractSystemsFinder::SystemDescriptionList QnRecentLocalSystemsFinder::systems() const
{
    return m_finalSystems.values();
}

QnSystemDescriptionPtr QnRecentLocalSystemsFinder::getSystem(const QString &id) const
{
    return m_finalSystems.value(id, QnSystemDescriptionPtr());
}

void QnRecentLocalSystemsFinder::updateSystems()
{
    SystemsHash newSystems;
    const auto connections = qnClientCoreSettings->recentLocalConnections();
    for (auto it = connections.begin(); it != connections.end(); ++it)
    {
        if (it.key().isNull() || it->urls.isEmpty())
            continue;

        const auto connection = it.value();

        const bool hasOnlyCloudUrls = std::all_of(connection.urls.cbegin(), connection.urls.cend(),
            [](const nx::utils::Url& url)
            {
                return nx::network::SocketGlobals::addressResolver().isCloudHostName(url.host());
            });

        if (hasOnlyCloudUrls)
            continue;

        const auto system = QnLocalSystemDescription::create(
            it.key().toString(), it.key(), connection.systemName);

        static const int kVeryFarPriority = 100000;

        QnModuleInformation fakeServerInfo;
        fakeServerInfo.id = QnUuid::createUuid();   // It SHOULD be new unique id
        fakeServerInfo.systemName = connection.systemName;
        system->addServer(fakeServerInfo, kVeryFarPriority, false);
        system->setServerHost(fakeServerInfo.id, connection.urls.first());
        newSystems.insert(system->id(), system);
    }

    const auto newFinalSystems = filterOutSystems(newSystems);
    setFinalSystems(newFinalSystems);
}

void QnRecentLocalSystemsFinder::removeFinalSystem(const QString& id)
{
    if (m_finalSystems.remove(id))
        emit systemLost(id);
}

void QnRecentLocalSystemsFinder::processSystemAdded(const QnSystemDescriptionPtr& system)
{
    const auto systemId = system->id();
    auto& hash = m_filteringSystems[systemId];
    auto& localIdUsageCount = hash[system->localId()];
    ++localIdUsageCount;

    QSet<QString> forRemove;
    const auto localId = system->localId();
    for (const auto& finalSystem: m_finalSystems)
    {
        if (!isAcceptablePred(finalSystem, systemId, localId))
            forRemove.insert(finalSystem->id());
    }

    for (const auto& id: forRemove)
        removeFinalSystem(id);
}

void QnRecentLocalSystemsFinder::processSystemRemoved(
    const QString& systemId,
    const QnUuid& localSystemId)
{
    const auto it = m_filteringSystems.find(systemId);
    if (it == m_filteringSystems.end())
        return;

    auto& hash = it.value();
    const auto itLocalData = hash.find(localSystemId);
    if (itLocalData == hash.end())
        return;

    auto& localIdUsageCount = itLocalData.value();
    if (--localIdUsageCount)
        return;

    hash.erase(itLocalData);
    if (!hash.isEmpty())
        return;

    m_filteringSystems.erase(it);
    updateSystems();
}


QnRecentLocalSystemsFinder::SystemsHash
    QnRecentLocalSystemsFinder::filterOutSystems(const SystemsHash& source)
{
    const auto isAcceptable =
        [this](const QnSystemDescriptionPtr& system)
        {
            // Filters out all systems with local id that exists in discovered systems
            for (auto it = m_filteringSystems.begin(); it != m_filteringSystems.end(); ++it)
            {
                const auto systemId = it.key();
                const auto hash = it.value();
                for (const auto& localId: hash.keys())
                {
                    if (!isAcceptablePred(system, systemId, localId))
                        return false;
                }
            }
            return true;
        };

    SystemsHash result;
    for (const auto& unfiltered: source)
    {
        if (isAcceptable(unfiltered))
            result.insert(unfiltered->id(), unfiltered);
    }
    return result;
}

void QnRecentLocalSystemsFinder::setFinalSystems(const SystemsHash& newFinalSystems)
{
    if (m_finalSystems == newFinalSystems)
        return;

    const auto newSystemsKeys = newFinalSystems.keys().toSet();
    const auto currentKeys = m_finalSystems.keys().toSet();
    const auto added = newSystemsKeys - currentKeys;
    const auto removed = currentKeys - newSystemsKeys;

    for (const auto& systemId: removed)
        removeFinalSystem(systemId);

    for (const auto& systemId: added)
    {
        const auto system = newFinalSystems.value(systemId);
        m_finalSystems.insert(systemId, system);
        emit systemDiscovered(system);
    }
}
