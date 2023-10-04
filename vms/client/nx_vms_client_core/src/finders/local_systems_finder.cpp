// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "local_systems_finder.h"

#include <nx/utils/qt_helpers.h>

namespace {

const auto isAcceptablePred =
    [](const QnSystemDescriptionPtr& system, const QnUuid& localId)
    {
        // Filters out all systems with local id that exists in discovered systems
        return localId != system->localId();
    };

} // namespace

LocalSystemsFinder::LocalSystemsFinder(QObject* parent):
    base_type(parent)
{
}

QnAbstractSystemsFinder::SystemDescriptionList LocalSystemsFinder::systems() const
{
    return m_filteredSystems.values();
}

QnSystemDescriptionPtr LocalSystemsFinder::getSystem(const QString& id) const
{
    const auto localIdIter = m_systemToLocalId.find(id);
    return localIdIter != m_systemToLocalId.end()
        ? m_filteredSystems.value(*localIdIter, QnSystemDescriptionPtr())
        : QnSystemDescriptionPtr();
}

void LocalSystemsFinder::processSystemAdded(const QnSystemDescriptionPtr& system)
{
    const QnUuid localId = system->localId();
    auto& idCountInfo = m_nonLocalSystems[localId];
    auto& systemIdUsageCount = idCountInfo[system->id()];
    ++systemIdUsageCount;

    QSet<QnUuid> forRemove;
    for (const auto& system: m_filteredSystems)
    {
        if (!isAcceptablePred(system, localId))
            forRemove.insert(system->localId());
    }

    for (const QnUuid& localId: forRemove)
        removeFilteredSystem(localId);
}

void LocalSystemsFinder::processSystemRemoved(
    const QString& systemId,
    const QnUuid& localSystemId)
{
    const auto it = m_nonLocalSystems.find(localSystemId);
    if (it == m_nonLocalSystems.end())
        return;

    auto& idCountInfo = it.value();
    const auto itLocalData = idCountInfo.find(systemId);
    if (itLocalData == idCountInfo.end())
        return;

    auto& localIdUsageCount = itLocalData.value();
    if (--localIdUsageCount)
        return;

    idCountInfo.erase(itLocalData);
    if (!idCountInfo.isEmpty())
        return;

    m_nonLocalSystems.erase(it);
    updateSystems();
}

void LocalSystemsFinder::setSystems(const SystemsHash& newSystems)
{
    const auto newFilteredSystems = filterOutSystems(newSystems);
    if (m_filteredSystems == newFilteredSystems)
        return;

    const auto newSystemsKeys = nx::utils::toQSet(newFilteredSystems.keys());
    const auto currentKeys = nx::utils::toQSet(m_filteredSystems.keys());
    const auto added = newSystemsKeys - currentKeys;
    const auto removed = currentKeys - newSystemsKeys;

    for (const QnUuid& localId: removed)
        removeFilteredSystem(localId);

    for (const QnUuid& localId: added)
    {
        const auto system = newFilteredSystems.value(localId);
        m_systemToLocalId[newFilteredSystems[localId]->id()] = localId;
        m_filteredSystems.insert(localId, system);
        emit systemDiscovered(system);
    }
}

LocalSystemsFinder::SystemsHash LocalSystemsFinder::filterOutSystems(const SystemsHash& source)
{
    const auto isAcceptable =
        [this](const QnSystemDescriptionPtr& system)
        {
            return !m_nonLocalSystems.contains(system->localId());
        };

    SystemsHash result;
    for (const auto& unfiltered: source)
    {
        if (isAcceptable(unfiltered))
            result.insert(unfiltered->localId(), unfiltered);
    }
    return result;
}

void LocalSystemsFinder::removeFilteredSystem(const QnUuid& localid)
{
    const auto systemIter = m_filteredSystems.find(localid);
    if (systemIter != m_filteredSystems.end())
    {
        const QString systemId = systemIter.value()->id();
        m_systemToLocalId.remove(systemId);
        m_filteredSystems.erase(systemIter);
        emit systemLost(systemId);
    }
}
