#include "status_dictionary.h"

Qn::ResourceStatus QnResourceStatusDictionary::value(const QnUuid& resourceId) const
{
    SCOPED_MUTEX_LOCK( lock, &m_mutex);
    auto itr = m_items.find(resourceId);
    return itr != m_items.end() ? itr.value() : Qn::Offline;
}

void QnResourceStatusDictionary::setValue(const QnUuid& resourceId, Qn::ResourceStatus status)
{
    assert(!resourceId.isNull());
    SCOPED_MUTEX_LOCK( lock, &m_mutex);
    m_items[resourceId] = status;
}

void QnResourceStatusDictionary::clear()
{
    SCOPED_MUTEX_LOCK( lock, &m_mutex);
    m_items.clear();
}

void QnResourceStatusDictionary::clear(const QVector<QnUuid>& idList)
{
    SCOPED_MUTEX_LOCK( lock, &m_mutex);
    for(const QnUuid& id: idList)
        m_items.remove(id);
}

QMap<QnUuid, Qn::ResourceStatus> QnResourceStatusDictionary::values() const
{
    SCOPED_MUTEX_LOCK( lock, &m_mutex);
    return m_items;
}
