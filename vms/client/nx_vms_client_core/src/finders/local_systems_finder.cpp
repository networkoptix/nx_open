// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "local_systems_finder.h"

#include <nx/utils/qt_helpers.h>

LocalSystemsFinder::LocalSystemsFinder(QObject* parent):
    base_type(parent),
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
