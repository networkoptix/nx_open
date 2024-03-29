// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "non_unique_name_tracker.h"

namespace nx::vms::client::desktop {

bool NonUniqueNameTracker::update(const nx::Uuid& id, const QString& name)
{
    bool changed = false;

    auto nameIt = m_nameById.find(id);
    if (nameIt != m_nameById.end())
    {
        if (*nameIt == name)
            return false;
        changed = remove(id);
    }

    m_nameById[id] = name;

    QSet<nx::Uuid>& ids = m_idsByName[name];
    ids.insert(id);

    if (ids.size() == 2)
    {
        for (const nx::Uuid& id: ids)
            m_nonUniqueNameIds.insert(id);
        changed = true;
    }
    else if (ids.size() > 2)
    {
        m_nonUniqueNameIds.insert(id);
        changed = true;
    }

    return changed;
}

bool NonUniqueNameTracker::remove(const nx::Uuid& id)
{
    auto nameIt = m_nameById.find(id);
    if (nameIt == m_nameById.end())
        return false;

    const QString name = *nameIt;
    m_nameById.erase(nameIt);

    bool changed = false;

    auto it = m_idsByName.find(name);
    switch (it->size())
    {
        case 1:
            m_idsByName.erase(it);
            break;
        case 2:
            for (const nx::Uuid& id: *it)
                m_nonUniqueNameIds.remove(id);
            [[fallthrough]];
        default:
            m_nonUniqueNameIds.remove(id);
            it->remove(id);
            changed = true;
    }

    return changed;
}

QSet<nx::Uuid> NonUniqueNameTracker::idsByName(const QString& name) const
{
    return m_idsByName.value(name.toLower());
}

} // namespace nx::vms::client::desktop
