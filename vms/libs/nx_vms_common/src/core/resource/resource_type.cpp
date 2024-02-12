// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_type.h"

#include <QtCore/QDebug>

#include <nx/utils/log/log.h>
#include <nx/vms/api/data/media_server_data.h>
#include <nx/vms/api/data/user_data.h>

const QString QnResourceTypePool::kC2pCameraTypeId("C2pCamera");

QnResourceType::~QnResourceType()
{
}

void QnResourceType::setParentId(const nx::Uuid &value)
{
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

    for (const nx::Uuid& parentId: allParentList())
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

void QnResourceType::setIsCamera()
{
    m_isCamera = true;
    m_isCameraSet = true;
}

void QnResourceType::addAdditionalParent(const nx::Uuid& parent)
{
    if (parent.isNull())
    {
        qWarning() << "Adding NULL parentId";
        return;
    }

    if (parent != m_parentId)
        m_additionalParentList << parent;
}

QList<nx::Uuid> QnResourceType::allParentList() const
{
    QList<nx::Uuid> result;
    if (!m_parentId.isNull())
        result << m_parentId;
    result << m_additionalParentList;
    return result;
}

void QnResourceType::addParamType(const QString& name, const QString& defaultValue)
{
    NX_MUTEX_LOCKER lock(&m_allParamTypeListCacheMutex); //< in case of connect to another app server
    m_paramTypeList.insert(name, defaultValue);
}

bool QnResourceType::hasParam(const QString& name) const
{
    NX_MUTEX_LOCKER lock(&m_allParamTypeListCacheMutex);
    return paramTypeListUnsafe().contains(name);
}

QString QnResourceType::defaultValue(const QString& key) const
{
    NX_MUTEX_LOCKER lock(&m_allParamTypeListCacheMutex);
    return paramTypeListUnsafe().value(key);
}

const QnResourceType::ParamTypeMap QnResourceType::paramTypeList() const
{
    NX_MUTEX_LOCKER lock(&m_allParamTypeListCacheMutex);
    return paramTypeListUnsafe();
}

const QnResourceType::ParamTypeMap& QnResourceType::paramTypeListUnsafe() const
{
    if (m_allParamTypeListCache.isNull())
    {
        QSharedPointer<ParamTypeMap> allParamTypeListCache(new ParamTypeMap());
        *allParamTypeListCache = m_paramTypeList;

        for (const nx::Uuid& parentId: allParentList())
        {
            if (parentId.isNull())
            {
                continue;
            }

            if (QnResourceTypePtr parent = qnResTypePool->getResourceType(parentId))
            {   // Note. Copy below, should be thread safe.
                ParamTypeMap parentData = parent->paramTypeList();
                for(auto itr = parentData.begin(); itr != parentData.end(); ++itr)
                {
                    if (!allParamTypeListCache->contains(itr.key()))
                        allParamTypeListCache->insert(itr.key(), itr.value());
                }
            }
            else
            {
                qWarning() << "parentId is" << parentId.toString() << "but there is no such parent in database";
            }
        }

        m_allParamTypeListCache = allParamTypeListCache;
    }

    return *m_allParamTypeListCache.data();
}

// =============================== QnResourceTypePool ========================

QnResourceTypePool *QnResourceTypePool::instance()
{
    static QnResourceTypePool staticInstance;
    return &staticInstance;
}

QnResourceTypePool::QnResourceTypePool()
{
    // Add static data for predefined types.
    // It needs for UT only. Media server load it from the database as well.
    QnResourceTypePtr resType(new QnResourceType());
    resType->setName(nx::vms::api::UserData::kResourceTypeName);
    resType->setId(nx::vms::api::UserData::kResourceTypeId);
    addResourceType(resType);
}

QnResourceTypePtr QnResourceTypePool::getResourceTypeByName(const QString& name) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    for (auto itr = m_resourceTypeMap.begin(); itr != m_resourceTypeMap.end(); ++itr)
    {
        if (itr.value()->getName() == name)
            return itr.value();
    }
    return QnResourceTypePtr();
}

QnResourceTypePtr QnResourceTypePool::getResourceType(nx::Uuid id) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    auto itr = m_resourceTypeMap.find(id);
    return itr != m_resourceTypeMap.end() ? itr.value() : QnResourceTypePtr();
}

void QnResourceTypePool::replaceResourceTypeList(const QnResourceTypeList &resourceTypeList)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    m_resourceTypeMap.clear();

    for (const QnResourceTypePtr& resourceType: resourceTypeList)
        m_resourceTypeMap.insert(resourceType->getId(), resourceType);
}

void QnResourceTypePool::addResourceType(QnResourceTypePtr resourceType)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_resourceTypeMap.insert(resourceType->getId(), resourceType);
}

nx::Uuid QnResourceTypePool::getResourceTypeId(const QString& manufacturer, const QString& name, bool showWarning) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    for (const QnResourceTypePtr& rt: m_resourceTypeMap)
    {
        if (rt->getManufacturer() == manufacturer && rt->getName().compare(name, Qt::CaseInsensitive) == 0)
            return rt->getId();
    }

    if (showWarning)
    {
        NX_VERBOSE(this, "Cannot find resource type for manufacturer: %1, model name: %2",
            manufacturer, name);
    }

    return nx::Uuid();
}

nx::Uuid QnResourceTypePool::getLikeResourceTypeId(
    const QString& manufacturer, const QString& name) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    nx::Uuid result;
    int bestLen = -1;
    for (const QnResourceTypePtr& rt: m_resourceTypeMap)
    {
        if (rt->getManufacturer() == manufacturer)
        {
            int len = rt->getName().length();
            if (len > bestLen && rt->getName() == name.left(len))
            {
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
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_resourceTypeMap.isEmpty();
}

QnResourceTypePool::QnResourceTypeMap QnResourceTypePool::getResourceTypeMap() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return QnResourceTypeMap(m_resourceTypeMap);
}
