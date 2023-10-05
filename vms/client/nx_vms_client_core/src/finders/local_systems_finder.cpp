// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "local_systems_finder.h"

#include <nx/utils/qt_helpers.h>

LocalSystemsFinder::LocalSystemsFinder(QObject* parent):
    base_type(parent)
{
}

QnAbstractSystemsFinder::SystemDescriptionList LocalSystemsFinder::systems() const
{
    return m_systems.values();
}

QnSystemDescriptionPtr LocalSystemsFinder::getSystem(const QString& id) const
{
    return m_systems.value(id, QnSystemDescriptionPtr());
}

void LocalSystemsFinder::setSystems(const SystemsHash& newSystems)
{
    if (m_systems == newSystems)
        return;

    const auto newSystemsKeys = nx::utils::toQSet(newSystems.keys());
    const auto currentKeys = nx::utils::toQSet(m_systems.keys());
    const auto added = newSystemsKeys - currentKeys;
    const auto removed = currentKeys - newSystemsKeys;

    for (const auto& systemId: removed)
        removeSystem(systemId);

    for (const auto& systemId: added)
    {
        const auto system = newSystems.value(systemId);
        m_systems.insert(systemId, system);
        emit systemDiscovered(system);
    }
}

void LocalSystemsFinder::removeSystem(const QString& id)
{
    if (m_systems.remove(id))
        emit systemLost(id);
}
