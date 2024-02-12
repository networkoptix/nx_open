// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "save_state_manager.h"

namespace nx::vms::client::core {

SaveStateManager::SaveStateManager(QObject* parent):
    base_type(parent),
    m_mutex(nx::Mutex::NonRecursive)
{

}

SaveStateManager::SaveStateFlags SaveStateManager::flags(const nx::Uuid& id) const
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    return m_flags.value(id);
}

void SaveStateManager::setFlags(const nx::Uuid& id, SaveStateFlags flags)
{
    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        if (m_flags.value(id) == flags)
            return;

        m_flags[id] = flags;
    }
    emit flagsChanged(id, flags);
}

void SaveStateManager::clean(const nx::Uuid& id)
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    m_flags.remove(id);
    m_saveRequests.remove(id);
}

bool SaveStateManager::isBeingSaved(const nx::Uuid& id) const
{
    return flags(id).testFlag(IsBeingSaved);
}

bool SaveStateManager::isChanged(const nx::Uuid& id) const
{
    return flags(id).testFlag(IsChanged);
}

bool SaveStateManager::isSaveable(const nx::Uuid& id) const
{
    return (flags(id) & (IsChanged | IsBeingSaved)) == IsChanged;
}

void SaveStateManager::markBeingSaved(const nx::Uuid& id, bool saved)
{
    if (saved)
        setFlags(id, flags(id) | IsBeingSaved);
    else
        setFlags(id, flags(id) & ~IsBeingSaved);
}

void SaveStateManager::markChanged(const nx::Uuid& id, bool changed)
{
    if (changed)
        setFlags(id, flags(id) | IsChanged);
    else
        setFlags(id, flags(id) & ~IsChanged);
}

bool SaveStateManager::hasSaveRequests(const nx::Uuid& id) const
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    return !m_saveRequests.value(id).empty();
}

void SaveStateManager::addSaveRequest(const nx::Uuid& id, int reqId)
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    m_saveRequests[id].insert(reqId);
}

void SaveStateManager::removeSaveRequest(const nx::Uuid& id, int reqId)
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    if (!m_saveRequests.contains(id))
        return;

    auto& ids = m_saveRequests[id];
    ids.remove(reqId);
    if (ids.empty())
        m_saveRequests.remove(id);
}

} // namespace nx::vms::client::core
