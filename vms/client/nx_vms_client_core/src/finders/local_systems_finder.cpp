// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "local_systems_finder.h"

#include <nx/utils/qset.h>

namespace {

const auto isAcceptablePred =
    [](const QnSystemDescriptionPtr& system, const QString& systemId, const QnUuid& localId)
    {
        // Filters out all systems with local id that exists in discovered systems
        return ((systemId != system->id()) && (localId.toString() != system->id()));
    };

} // namespace

LocalSystemsFinder::LocalSystemsFinder(QObject* parent):
    base_type(parent),
    m_filteringSystems(),
    m_finalSystems()
{
}

QnAbstractSystemsFinder::SystemDescriptionList LocalSystemsFinder::systems() const
{
    return m_finalSystems.values();
}

QnSystemDescriptionPtr LocalSystemsFinder::getSystem(const QString &id) const
{
    return m_finalSystems.value(id, QnSystemDescriptionPtr());
}

void LocalSystemsFinder::removeFinalSystem(const QString& id)
{
    if (m_finalSystems.remove(id))
        emit systemLost(id);
}

void LocalSystemsFinder::processSystemAdded(const QnSystemDescriptionPtr& system)
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

void LocalSystemsFinder::processSystemRemoved(
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


LocalSystemsFinder::SystemsHash
    LocalSystemsFinder::filterOutSystems(const SystemsHash& source)
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

void LocalSystemsFinder::setFinalSystems(const SystemsHash& newFinalSystems)
{
    if (m_finalSystems == newFinalSystems)
        return;

    const auto newSystemsKeys = nx::utils::toQSet(newFinalSystems.keys());
    const auto currentKeys = nx::utils::toQSet(m_finalSystems.keys());
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
