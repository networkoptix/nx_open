// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_save_state_manager.h"

QnAbstractSaveStateManager::QnAbstractSaveStateManager(QObject* parent):
    base_type(parent),
    m_mutex(nx::Mutex::NonRecursive)
{

}

QnAbstractSaveStateManager::SaveStateFlags QnAbstractSaveStateManager::flags(const QnUuid& id) const
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    return m_flags.value(id);
}

void QnAbstractSaveStateManager::setFlags(const QnUuid& id, SaveStateFlags flags)
{
    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        if (m_flags.value(id) == flags)
            return;

        m_flags[id] = flags;
    }
    emit flagsChanged(id, flags);
}

void QnAbstractSaveStateManager::clean(const QnUuid& id)
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    m_flags.remove(id);
    m_saveRequests.remove(id);
}

bool QnAbstractSaveStateManager::isBeingSaved(const QnUuid& id) const
{
    return flags(id).testFlag(IsBeingSaved);
}

bool QnAbstractSaveStateManager::isChanged(const QnUuid& id) const
{
    return flags(id).testFlag(IsChanged);
}

bool QnAbstractSaveStateManager::isSaveable(const QnUuid& id) const
{
    return (flags(id) & (IsChanged | IsBeingSaved)) == IsChanged;
}

void QnAbstractSaveStateManager::markBeingSaved(const QnUuid& id, bool saved)
{
    if (saved)
        setFlags(id, flags(id) | IsBeingSaved);
    else
        setFlags(id, flags(id) & ~IsBeingSaved);
}

void QnAbstractSaveStateManager::markChanged(const QnUuid& id, bool changed)
{
    if (changed)
        setFlags(id, flags(id) | IsChanged);
    else
        setFlags(id, flags(id) & ~IsChanged);
}

bool QnAbstractSaveStateManager::hasSaveRequests(const QnUuid& id) const
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    return !m_saveRequests.value(id).empty();
}

void QnAbstractSaveStateManager::addSaveRequest(const QnUuid& id, int reqId)
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    m_saveRequests[id].insert(reqId);
}

void QnAbstractSaveStateManager::removeSaveRequest(const QnUuid& id, int reqId)
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    if (!m_saveRequests.contains(id))
        return;
    auto& ids = m_saveRequests[id];
    ids.remove(reqId);
    if (ids.empty())
        m_saveRequests.remove(id);
}
