#include "resource_type.h"

Q_GLOBAL_STATIC(QnResourceTypePool, inst)

void QnResourceType::addAdditionalParent(const QnId& parent) 
{
    if (parent != m_parentId)
        m_additionalParentList << parent; 
}
QList<QnId> QnResourceType::allParentList() const {
    QList<QnId> result;
    result << m_parentId;
    result << m_additionalParentList;
    return result;
}

void QnResourceType::addParamType(QnParamTypePtr param)
{
    m_paramTypeList.append(param);
}

// =============================== QnResourceTypePool ========================

QnResourceTypePool* QnResourceTypePool::instance()
{
    return inst();
}

QnResourceTypePtr QnResourceTypePool::getResourceType(const QnId& id)
{
    QMutexLocker lock(&m_mutex);
    QnResourceTypeMap::iterator itr = m_resourceTypeMap.find(id);
    return itr != m_resourceTypeMap.end() ? itr.value() : QnResourceTypePtr();
}

void QnResourceTypePool::addResourceType(QnResourceTypePtr resourceType)
{
    QMutexLocker lock(&m_mutex);
    m_resourceTypeMap.insert(resourceType->getId(), resourceType);
}
