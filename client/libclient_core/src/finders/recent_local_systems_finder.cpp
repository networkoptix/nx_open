#include "recent_local_systems_finder.h"

#include <client_core/client_core_settings.h>
#include <client_core/local_connection_data.h>
#include <nx/network/http/asynchttpclient.h>

namespace {

const auto isAcceptablePred =
    [](const QnSystemDescriptionPtr& system, const QString& systemId, const QnUuid& localId)
{
    // Filters out all systems with local id that exists in discovered systems
    return ((systemId != system->id()) && (localId.toString() != system->id()));
};

}

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
    for (const auto connection : qnClientCoreSettings->recentLocalConnections())
    {
        if (connection.localId.isNull())
            continue;

        const auto system = QnSystemDescription::createLocalSystem(
            connection.localId.toString(), connection.localId, connection.systemName);

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
    auto it = m_filteringSystems.find(systemId);
    if (it == m_filteringSystems.end())
        it = m_filteringSystems.insert(systemId, IdCountPair(system->localId(), 0));

    auto& localIdUsageCount = it.value().second;
    ++localIdUsageCount;

    QSet<QString> forRemove;
    const auto localId = system->localId();
    for (const auto finalSystem : m_finalSystems)
    {
        if (!isAcceptablePred(finalSystem, systemId, localId))
            forRemove.insert(finalSystem->id());
    }

    for (const auto id : forRemove)
        removeFinalSystem(id);
}

void QnRecentLocalSystemsFinder::processSystemRemoved(const QString& systemId)
{
    const auto it = m_filteringSystems.find(systemId);
    if (it == m_filteringSystems.end())
        return;

    auto& localIdUsageCount = it.value().second;
    if (--localIdUsageCount)
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
                const auto localId = it.value().first;
                if (!isAcceptablePred(system, systemId, localId))
                    return false;
            }
            return true;
        };

    SystemsHash result;
    for (const auto unfiltered : source)
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

    for (const auto systemId : added)
    {
        const auto system = newFinalSystems.value(systemId);
        m_finalSystems.insert(systemId, system);
        emit systemDiscovered(system);
    }

    for (const auto systemId : removed)
        removeFinalSystem(systemId);
}
