#include "resource_type.h"
#include "utils/common/log.h"

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

QnResourceTypePtr QnResourceTypePool::getResourceType(const QnId& id) const
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

QnId QnResourceTypePool::getResourceTypeId(const QString& manufacture, const QString& name) const
{
    QMutexLocker lock(&m_mutex);
    foreach(QnResourceTypePtr rt, m_resourceTypeMap)
    {
        cl_log.log(rt->getName(), cl_logALWAYS); //debug

        if (rt->getName() == name && rt->getManufacture()==manufacture)
            return rt->getId();
    }

    Q_ASSERT(false);
    return QnId();
}