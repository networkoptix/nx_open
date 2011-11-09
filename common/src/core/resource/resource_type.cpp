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

const QList<QnParamTypePtr>& QnResourceType::paramTypeList() const
{
    if (m_allParamTypeListCache.isNull())
    {
        QMutexLocker _lock(&m_allParamTypeListCacheMutex);

        if (!m_allParamTypeListCache.isNull())
            return *(m_allParamTypeListCache.data());

        m_allParamTypeListCache = QSharedPointer<ParamTypeList>(new ParamTypeList());

        ParamTypeList paramTypeList = ParamTypeList();

        paramTypeList += m_paramTypeList;

        foreach(const QnId& parentId, allParentList())
        {
            QnResourceTypePtr parent = qnResTypePool->getResourceType(parentId);

            if (!parent.isNull())
                paramTypeList += parent->paramTypeList();
        }

        QSet<QString> paramTypeNames;

        QList<QnParamTypePtr>::iterator it = paramTypeList.begin();
        for (; it != paramTypeList.end(); ++it)
        {
            const QnParamTypePtr& paramType = *it;

            if (!paramTypeNames.contains(paramType->name))
            {
                m_allParamTypeListCache->append(paramType);
                paramTypeNames.insert(paramType->name);
            }
        }
    }

    return *m_allParamTypeListCache.data();
}
// =============================== QnResourceTypePool ========================

QnResourceTypePool* QnResourceTypePool::instance()
{
    return inst();
}

QnResourceTypePtr QnResourceTypePool::getResourceType(const QnId& id) const
{
    QMutexLocker lock(&m_mutex);
    QnResourceTypeMap::const_iterator itr = m_resourceTypeMap.find(id);
    return itr != m_resourceTypeMap.end() ? itr.value() : QnResourceTypePtr();
}

void QnResourceTypePool::addResourceTypeList(const QList<QnResourceTypePtr>& resourceTypeList)
{
    QMutexLocker lock(&m_mutex);
    foreach(const QnResourceTypePtr& resourceType, resourceTypeList)
        m_resourceTypeMap.insert(resourceType->getId(), resourceType);
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
        //cl_log.log(rt->getName(), cl_logALWAYS); //debug

        if (rt->getName() == name && rt->getManufacture()==manufacture)
            return rt->getId();
    }

    Q_ASSERT(false);
    return QnId();
}
