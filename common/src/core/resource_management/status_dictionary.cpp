#include "status_dictionary.h"

Qn::ResourceStatus QnResourceStatusDiscionary::value(const QnUuid& resourceId) const
{
    QMutexLocker lock(&m_mutex);
    auto itr = m_items.find(resourceId);
    return itr != m_items.end() ? itr.value() : Qn::NotDefined;
}

void QnResourceStatusDiscionary::setValue(const QnUuid& resourceId, Qn::ResourceStatus status)
{
    QMutexLocker lock(&m_mutex);
    m_items[resourceId] = status;
}

void QnResourceStatusDiscionary::clear()
{
    QMutexLocker lock(&m_mutex);
    m_items.clear();
}

void QnResourceStatusDiscionary::clear(const QVector<QnUuid>& idList)
{
    QMutexLocker lock(&m_mutex);
    for(const QnUuid& id: idList)
        m_items.remove(id);
}
