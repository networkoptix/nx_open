#include "recent_local_systems_finder.h"

#include <client_core/client_core_settings.h>
#include <client_core/local_connection_data.h>
#include <nx/network/http/asynchttpclient.h>

namespace {

const auto isAcceptablePred =
    [](const QnSystemDescriptionPtr& system,
        const QnSystemDescriptionPtr& filteringSystem)
{
    // Filters out all systems with local id that exists in discovered systems
    return ((filteringSystem->localId().toString() != system->id())
        && (filteringSystem->id() != system->id()));
};

}

QnRecentLocalSystemsFinder::QnRecentLocalSystemsFinder(QObject* parent) :
    base_type(parent),
    m_filteringSystems(),
    m_unfilteredSystems(),

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
    m_filteringSystems.insert(system->id(), system);

    QSet<QString> forRemove;
    for (const auto finalSystem : m_finalSystems)
    {
        if (!isAcceptablePred(finalSystem, system))
            forRemove.insert(finalSystem->id());
    }

    for (const auto id : forRemove)
        removeFinalSystem(id);
}

void QnRecentLocalSystemsFinder::processSystemRemoved(const QString& systemId)
{
    if (m_filteringSystems.remove(systemId))
        updateSystems();
}


QnRecentLocalSystemsFinder::SystemsHash
    QnRecentLocalSystemsFinder::filterOutSystems(const SystemsHash& source)
{
    const auto isAcceptable =
        [this](const QnSystemDescriptionPtr& system)
        {
            // Filters out all systems with local id that exists in discovered systems
            return std::all_of(m_filteringSystems.begin(), m_filteringSystems.end(),
                [system](const QnSystemDescriptionPtr& value)
                {
                    return isAcceptablePred(system, value);
                });
        };

    SystemsHash result;
    for (const auto unfiltered : m_unfilteredSystems)
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
