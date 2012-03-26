#include "resource_type.h"

#include "utils/common/log.h"

QnResourceType::QnResourceType()
    : m_isCameraSet(false)
{
}

/*
QnResourceType::QnResourceType(const QString& name): m_name(name)
{
}
*/

QnResourceType::~QnResourceType()
{
}

bool QnResourceType::isCamera() const
{
    if (m_isCameraSet)
        return m_isCamera;

    if (m_name == "Camera")
    {
        m_isCamera = true;
        m_isCameraSet = true;

        return m_isCamera;
    }

    foreach (QnId parentId, allParentList())
    {
        if (parentId.isValid())
        {
            QnResourceTypePtr parent = qnResTypePool->getResourceType(parentId);
            if (parent->isCamera())
            {
                m_isCamera = true;
                m_isCameraSet = true;

                return m_isCamera;
            }
        }
    }

    m_isCamera = false;
    m_isCameraSet = true;

    return m_isCamera;
}

void QnResourceType::addAdditionalParent(QnId parent)
{
    if (parent != m_parentId)
        m_additionalParentList << parent;
}

QList<QnId> QnResourceType::allParentList() const
{
    QList<QnId> result;
    result << m_parentId;
    result << m_additionalParentList;
    return result;
}

void QnResourceType::addParamType(QnParamTypePtr param)
{
    QMutexLocker _lock(&m_allParamTypeListCacheMutex); // in case of connect to anther app server 
    m_paramTypeList.append(param);
}

const QList<QnParamTypePtr>& QnResourceType::paramTypeList() const
{
    if (m_allParamTypeListCache.isNull())
    {
        QMutexLocker _lock(&m_allParamTypeListCacheMutex);

        if (!m_allParamTypeListCache.isNull())
            return *(m_allParamTypeListCache.data());

        QSharedPointer<ParamTypeList> allParamTypeListCache(new ParamTypeList());

        ParamTypeList paramTypeList = ParamTypeList();

        paramTypeList += m_paramTypeList;

        foreach (QnId parentId, allParentList()) {
            if (QnResourceTypePtr parent = qnResTypePool->getResourceType(parentId))
                paramTypeList += parent->paramTypeList();
        }

        QSet<QString> paramTypeNames;

        QList<QnParamTypePtr>::iterator it = paramTypeList.begin();
        for (; it != paramTypeList.end(); ++it)
        {
            const QnParamTypePtr& paramType = *it;

            if (!paramTypeNames.contains(paramType->name))
            {
                allParamTypeListCache->append(paramType);
                paramTypeNames.insert(paramType->name);
            }
        }

        m_allParamTypeListCache = allParamTypeListCache;
    }

    return *m_allParamTypeListCache.data();
}

// =============================== QnResourceTypePool ========================

Q_GLOBAL_STATIC(QnResourceTypePool, theResourceTypePool)

QnResourceTypePool *QnResourceTypePool::instance()
{
    return theResourceTypePool();
}

QnResourceTypePtr QnResourceTypePool::getResourceType(QnId id) const
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

    cl_log.log("Cannot find such resource type!!!!: ", manufacture + QString(" ") + name, cl_logERROR);

    // Q_ASSERT(false);
    return QnId();
}

bool QnResourceTypePool::isEmpty() const
{
    QMutexLocker lock(&m_mutex);

    return m_resourceTypeMap.isEmpty();
}
