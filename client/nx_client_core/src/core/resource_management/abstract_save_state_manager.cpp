#include "abstract_save_state_manager.h"

QnAbstractSaveStateManager::QnAbstractSaveStateManager(QObject* parent):
    base_type(parent),
    m_mutex(QnMutex::NonRecursive)
{

}

QnAbstractSaveStateManager::SaveStateFlags QnAbstractSaveStateManager::flags(const QnUuid& id) const
{
    QnMutexLocker lk(&m_mutex);
    return m_flags.value(id);
}

void QnAbstractSaveStateManager::setFlags(const QnUuid& id, SaveStateFlags flags)
{
    {
        QnMutexLocker lk(&m_mutex);
        if (m_flags.value(id) == flags)
            return;

        m_flags[id] = flags;
    }
    emit flagsChanged(id, flags);
}

void QnAbstractSaveStateManager::clean(const QnUuid& id)
{
    QnMutexLocker lk(&m_mutex);
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
    QnMutexLocker lk(&m_mutex);
    return !m_saveRequests.value(id).empty();
}

void QnAbstractSaveStateManager::addSaveRequest(const QnUuid& id, int reqId)
{
    QnMutexLocker lk(&m_mutex);
    m_saveRequests[id].insert(reqId);
}

void QnAbstractSaveStateManager::removeSaveRequest(const QnUuid& id, int reqId)
{
    QnMutexLocker lk(&m_mutex);
    if (!m_saveRequests.contains(id))
        return;
    auto& ids = m_saveRequests[id];
    ids.remove(reqId);
    if (ids.empty())
        m_saveRequests.remove(id);
}
