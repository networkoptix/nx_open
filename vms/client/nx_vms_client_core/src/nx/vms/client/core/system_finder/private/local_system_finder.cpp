// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "local_system_finder.h"

#include <nx/utils/qt_helpers.h>

namespace nx::vms::client::core {

LocalSystemFinder::LocalSystemFinder(QObject* parent):
    base_type(parent)
{
}

SystemDescriptionList LocalSystemFinder::systems() const
{
    return m_systems.values();
}

SystemDescriptionPtr LocalSystemFinder::getSystem(const QString& id) const
{
    return m_systems.value(id, SystemDescriptionPtr());
}

void LocalSystemFinder::setSystems(const SystemsHash& newSystems)
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

void LocalSystemFinder::removeSystem(const QString& id)
{
    if (auto removed = m_systems.take(id))
        emit systemLost(id, removed->localId());
}

} // namespace nx::vms::client::core
