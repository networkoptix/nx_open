#include "resource_type.h"

#include <QtCore/QDebug>

#include <nx/utils/log/log.h>

const QString QnResourceTypePool::kLayoutTypeId(lit("Layout"));
const QString QnResourceTypePool::kServerTypeId(lit("Server"));
const QString QnResourceTypePool::kVideoWallTypeId(lit("Videowall"));
const QString QnResourceTypePool::kWebPageTypeId(lit("WebPage"));
const QString QnResourceTypePool::kStorageTypeId(lit("Storage"));
const QString QnResourceTypePool::kUserTypeId(lit("User"));

const QnUuid QnResourceTypePool::kUserTypeUuid(
    qnResTypePool->getFixedResourceTypeId(kUserTypeId));
const QnUuid QnResourceTypePool::kServerTypeUuid(
    qnResTypePool->getFixedResourceTypeId(kServerTypeId));
const QnUuid QnResourceTypePool::kStorageTypeUuid(
    qnResTypePool->getFixedResourceTypeId(kStorageTypeId));
const QnUuid QnResourceTypePool::kLayoutTypeUuid(
    qnResTypePool->getFixedResourceTypeId(kLayoutTypeId));
const QnUuid QnResourceTypePool::kDesktopCameraTypeUuid("{1657647e-f6e4-bc39-d5e8-563c93cb5e1c}");
const QnUuid QnResourceTypePool::kWearableCameraTypeUuid("{f7f5ab66-7075-4d0b-a0b2-75e2fdd079a4}");

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

void QnResourceType::setParentId(const QnUuid &value) {
    m_parentId = value;
}

bool QnResourceType::isCamera() const
{
    if (m_isCameraSet)
        return m_isCamera;

    if (m_name == QLatin1String("Camera"))
    {
        m_isCamera = true;
        m_isCameraSet = true;

        return m_isCamera;
    }

    for (const QnUuid& parentId: allParentList())
    {
        if (!parentId.isNull())
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

void QnResourceType::addAdditionalParent(const QnUuid& parent)
{
    if (parent.isNull()) {
        qWarning() << "Adding NULL parentId";
        return;
    }

    if (parent != m_parentId)
        m_additionalParentList << parent;
}

QList<QnUuid> QnResourceType::allParentList() const
{
    QList<QnUuid> result;
    if (!m_parentId.isNull())
        result << m_parentId;
    result << m_additionalParentList;
    return result;
}

void QnResourceType::addParamType(const QString& name, const QString& defaultValue)
{
    QnMutexLocker lock( &m_allParamTypeListCacheMutex ); // in case of connect to anther app server
    m_paramTypeList.insert(name, defaultValue);
}

bool QnResourceType::hasParam(const QString& name) const
{
    QnMutexLocker lock( &m_allParamTypeListCacheMutex );
    return paramTypeListUnsafe().contains(name);
}

QString QnResourceType::defaultValue(const QString& key) const
{
    QnMutexLocker lock( &m_allParamTypeListCacheMutex );
    return paramTypeListUnsafe().value(key);
}

const ParamTypeMap QnResourceType::paramTypeList() const
{
    QnMutexLocker lock( &m_allParamTypeListCacheMutex );
    return paramTypeListUnsafe();
}

const ParamTypeMap& QnResourceType::paramTypeListUnsafe() const
{
    if (m_allParamTypeListCache.isNull())
    {
        if (!m_allParamTypeListCache.isNull())
            return *(m_allParamTypeListCache.data());

        QSharedPointer<ParamTypeMap> allParamTypeListCache(new ParamTypeMap());
        *allParamTypeListCache = m_paramTypeList;

        for (const QnUuid& parentId: allParentList()) {
            if (parentId.isNull()) {
                continue;
            }

            if (QnResourceTypePtr parent = qnResTypePool->getResourceType(parentId))
            {   // Note. Copy below, should be thread safe.
                ParamTypeMap parentData = parent->paramTypeList();
                for(auto itr = parentData.begin(); itr != parentData.end(); ++itr) {
                    if (!allParamTypeListCache->contains(itr.key()))
                        allParamTypeListCache->insert(itr.key(), itr.value());
                }
            } else {
                qWarning() << "parentId is" << parentId.toString() << "but there is no such parent in database";
            }
        }

        m_allParamTypeListCache = allParamTypeListCache;
    }

    return *m_allParamTypeListCache.data();
}

// =============================== QnResourceTypePool ========================

Q_GLOBAL_STATIC(QnResourceTypePool, QnResourceTypePool_instance)

QnResourceTypePool *QnResourceTypePool::instance()
{
    return QnResourceTypePool_instance();
}

QnResourceTypePtr QnResourceTypePool::getResourceTypeByName(const QString& name) const
{
    QnMutexLocker lock( &m_mutex );
    for(QnResourceTypeMap::const_iterator itr = m_resourceTypeMap.begin(); itr != m_resourceTypeMap.end(); ++itr)
    {
        if (itr.value()->getName() == name)
            return itr.value();
    }
    return QnResourceTypePtr();
}

QnResourceTypePtr QnResourceTypePool::getResourceType(QnUuid id) const
{
    QnMutexLocker lock( &m_mutex );
    QnResourceTypeMap::const_iterator itr = m_resourceTypeMap.find(id);
    return itr != m_resourceTypeMap.end() ? itr.value() : QnResourceTypePtr();
}

void QnResourceTypePool::replaceResourceTypeList(const QnResourceTypeList &resourceTypeList)
{
    QnMutexLocker lock( &m_mutex );

    m_resourceTypeMap.clear();

    for(const QnResourceTypePtr& resourceType: resourceTypeList)
        m_resourceTypeMap.insert(resourceType->getId(), resourceType);
}

void QnResourceTypePool::addResourceType(QnResourceTypePtr resourceType)
{
    QnMutexLocker lock( &m_mutex );
    m_resourceTypeMap.insert(resourceType->getId(), resourceType);
}

QnUuid QnResourceTypePool::getResourceTypeId(const QString& manufacture, const QString& name, bool showWarning) const
{
    QnMutexLocker lock( &m_mutex );
    for(const QnResourceTypePtr& rt: m_resourceTypeMap)
    {
        if (rt->getManufacture()==manufacture && rt->getName().compare(name, Qt::CaseInsensitive) == 0)
            return rt->getId();
    }

    if (showWarning)
        NX_LOG( lit("Cannot find resource type for manufacturer: %1, model name: %2").arg(manufacture).arg(name), cl_logDEBUG2 );

    // NX_ASSERT(false);
    return QnUuid();
}

QnUuid QnResourceTypePool::getFixedResourceTypeId(const QString& name)
{
    QnUuid result = guidFromArbitraryData(name.toUtf8() + QByteArray("-"));

#ifdef _DEBUG
    QnUuid online = qnResTypePool->getResourceTypeId(QString(), name, false);
    if (!online.isNull())
        NX_ASSERT(result == online);
#endif

    return result;
}

QnUuid QnResourceTypePool::getLikeResourceTypeId(const QString& manufacture, const QString& name) const
{
    QnMutexLocker lock( &m_mutex );
    QnUuid result;
    int bestLen = -1;
    for(const QnResourceTypePtr& rt: m_resourceTypeMap)
    {
        if (rt->getManufacture() == manufacture)
        {
            int len = rt->getName().length();
            if (len > bestLen && rt->getName() == name.left(len)) {
                result = rt->getId();
                bestLen = len;
                if (len == name.length())
                    break;
            }
        }
    }

    return result;
}

bool QnResourceTypePool::isEmpty() const
{
    QnMutexLocker lock( &m_mutex );

    return m_resourceTypeMap.isEmpty();
}

QnResourceTypePool::QnResourceTypeMap QnResourceTypePool::getResourceTypeMap() const
{
    QnMutexLocker lock( &m_mutex );

    return QnResourceTypeMap(m_resourceTypeMap);
}
