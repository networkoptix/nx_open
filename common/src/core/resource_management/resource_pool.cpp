#include "resource_pool.h"

#include <algorithm>

#include <QtCore/QMetaObject>

#include <core/resource/media_server_resource.h>
#include <core/resource/network_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/videowall_item.h>
#include <core/resource/videowall_item_index.h>
#include <core/resource/videowall_matrix_index.h>

#include <utils/common/warnings.h>
#include <utils/common/checked_cast.h>

#ifdef QN_RESOURCE_POOL_DEBUG
#   define TRACE(...) qDebug << __VA_ARGS__;
#else
#   define TRACE(...)
#endif

static const QString kLiteClientLayoutKey = lit("liteClient");

QnResourcePool::QnResourcePool(QObject* parent):
    base_type(parent),
    QnCommonModuleAware(parent),
    m_resourcesMtx(QnMutex::Recursive),
    m_tranInProgress(false)
{
    invalidateCache();
}

QnResourcePool::~QnResourcePool()
{
    bool signalsBlocked = blockSignals(false);
    //emit aboutToBeDestroyed();
    blockSignals(signalsBlocked);

    QnMutexLocker locker( &m_resourcesMtx );
    m_adminResource.clear();
    m_resources.clear();
}

void QnResourcePool::beginTran()
{
    m_tranInProgress = true;
}

void QnResourcePool::commit()
{
    m_tranInProgress = false;
    addResources(m_tmpResources);
    m_tmpResources.clear();
}

void QnResourcePool::addResource(const QnResourcePtr &resource)
{
    if (m_tranInProgress)
        m_tmpResources << resource;
    else
        addResources(QnResourceList() << resource);
}

void QnResourcePool::addIncompatibleResource(const QnResourcePtr &resource)
{
    addResources(QnResourceList() << resource, false);
}

void QnResourcePool::addResources(const QnResourceList& resources, bool mainPool)
{
    QnMutexLocker resourcesLock( &m_resourcesMtx );

    for (const QnResourcePtr &resource: resources)
    {
        NX_ASSERT(resource->toSharedPointer()); /* Getting an NX_ASSERT here? Did you forget to use QnSharedResourcePointer? */
        NX_ASSERT(!resource->getId().isNull());

        if(resource->resourcePool() != NULL)
            qnWarning("Given resource '%1' is already in the pool.", resource->metaObject()->className());
        resource->setResourcePool(this);
    }

    QMap<QnUuid, QnResourcePtr> newResources; // sort by id

    for (const QnResourcePtr &resource: resources)
    {
        if (resource->getId().isNull())
        {
            // ignore invalid resource in release mode
            qWarning() << "Got resource with empty ID. ignoring."
                       << "type=" << resource->getTypeId().toString()
                       << "name=" << resource->getName()
                       << "url=" << resource->getUrl();
            continue;
        }

        if( insertOrUpdateResource(
                resource,
                mainPool ? &m_resources : &m_incompatibleResources) )
        {
            newResources.insert(resource->getId(), resource);
        }

        if (mainPool)
            m_incompatibleResources.remove(resource->getId());
    }

    resourcesLock.unlock();

    QnResourceList addedResources = newResources.values();
    for (const auto& resource: addedResources)
    {
#ifdef DESKTOP_CAMERA_DEBUG
        if (resource.dynamicCast<QnNetworkResource>() &&
            resource->getTypeId() == qnResTypePool->desktopCameraResourceType()->getId()) {
            qDebug() << "desktop camera added to resource pool" << resource->getName() << resource.dynamicCast<QnNetworkResource>()->getPhysicalId();
            connect(resource, &QnResource::statusChanged, this, [this, resource] {
                qDebug() << "desktop camera status changed" << resource->getName() << resource.dynamicCast<QnNetworkResource>()->getPhysicalId() << resource->getStatus();
            });
        }
#endif

        connect(resource, &QnResource::statusChanged,      this, &QnResourcePool::statusChanged,     Qt::QueuedConnection);
        connect(resource, &QnResource::statusChanged,      this, &QnResourcePool::resourceChanged,   Qt::QueuedConnection);
        connect(resource, &QnResource::resourceChanged,    this, &QnResourcePool::resourceChanged,   Qt::QueuedConnection);

        if (!resource->hasFlags(Qn::foreigner))
        {
            if (resource->getStatus() != Qn::Offline)
                resource->initAsync(false);
        }
    }


    for (const auto& resource: addedResources)
    {
        TRACE("RESOURCE ADDED" << resource->metaObject()->className() << resource->getName());
        //emit resourceAdded(resource);
    }
}

void QnResourcePool::removeResources(const QnResourceList& resources)
{
    QnResourceList removedLayoutResources;
    QnResourceList removedUsers;
    QnResourceList removedOtherResources;
    auto appendRemovedResource = [&](QnResourcePtr resource)
    {
        if (resource->hasFlags(Qn::layout))
            removedLayoutResources.push_back(resource);
        else if (resource->hasFlags(Qn::user))
            removedUsers.push_back(resource);
        else
            removedOtherResources.push_back(resource);
    };

    QnMutexLocker lk( &m_resourcesMtx );

    for (const QnResourcePtr &resource: resources)
    {
        if (!resource)
            continue;

        disconnect(resource, nullptr, this, nullptr);

        resource->setRemovedFromPool(true);
        if (resource->resourcePool() != this)
            qnWarning("Given resource '%1' is not in the pool", resource->metaObject()->className());

#ifdef DESKTOP_CAMERA_DEBUG
        if (resource.dynamicCast<QnNetworkResource>() &&
            resource->getTypeId() == qnResTypePool->desktopCameraResourceType()->getId()) {
                qDebug() << "desktop camera removed from resource pool" << resource->getName() << resource.dynamicCast<QnNetworkResource>()->getPhysicalId();
        }
#endif

        //const QString& uniqueId = resource->getUniqueId();
        //if( m_resources.remove(uniqueId) != 0 )
        //    removedResources.append(resource);

        //have to remove by id, since uniqueId can be MAC and, as a result, not unique among friend and foreign resources
        QnUuid resId = resource->getId();
        QHash<QnUuid, QnResourcePtr>::iterator resIter = m_resources.find(resId);
        if (m_adminResource && resId == m_adminResource->getId())
            m_adminResource.clear();

        if (resIter != m_resources.end())
        {
            m_resources.erase(resIter);
            invalidateCache();
            appendRemovedResource(resource);
        }
        else
        {
            resIter = m_incompatibleResources.find(resource->getId());
            if (resIter != m_incompatibleResources.end())
            {
                m_incompatibleResources.erase(resIter);
                invalidateCache();
                appendRemovedResource(resource);
            }
        }

        resource->setResourcePool(nullptr);
    }

    /* Remove layout resources. */
    const auto videoWalls = getResources<QnVideoWallResource>();
    for (const QnResourcePtr& layoutResource : removedLayoutResources)
    {
        for (const QnVideoWallResourcePtr& videowall : videoWalls) // TODO: #Elric this is way beyond what one may call 'suboptimal'.
        {
            for (QnVideoWallItem item : videowall->items()->getItems())
            {
                if (item.layout != layoutResource->getId())
                    continue;
                item.layout = QnUuid();
                videowall->items()->updateItem(item);
            }
        }

        TRACE("RESOURCE REMOVED" << layoutResource->metaObject()->className() << layoutResource->getName());
    }

    /* Remove other resources. */
    const auto layouts = getResources<QnLayoutResource>();
    for (const QnResourcePtr& otherResource : removedOtherResources)
    {
        for (const QnLayoutResourcePtr& layoutResource : layouts) // TODO: #Elric this is way beyond what one may call 'suboptimal'.
            for (const QnLayoutItemData& data: layoutResource->getItems())
                if (data.resource.id == otherResource->getId() || data.resource.uniqueId == otherResource->getUniqueId())
                    layoutResource->removeItem(data);

        TRACE("RESOURCE REMOVED" << otherResource->metaObject()->className() << otherResource->getName());
    }

    lk.unlock();

    /* Emit notifications. */
    for (auto layoutResource: removedLayoutResources)
        emit resourceRemoved(layoutResource);

    for (auto resource : removedOtherResources)
        emit resourceRemoved(resource);

    for (auto user: removedUsers)
        emit resourceRemoved(user);
}

QnResourceList QnResourcePool::getResources() const
{
    QnMutexLocker locker( &m_resourcesMtx );
    return m_resources.values();
}

QnResourcePtr QnResourcePool::getResourceById(const QnUuid &id) const {
    QnMutexLocker locker( &m_resourcesMtx );

    QHash<QnUuid, QnResourcePtr>::const_iterator resIter = m_resources.find(id);
    if( resIter != m_resources.end() )
        return resIter.value();

    return QnResourcePtr(NULL);
}

QnResourcePtr QnResourcePool::getResourceByUrl(const QString &url) const
{
    QnMutexLocker locker( &m_resourcesMtx );
    for (const QnResourcePtr &resource: m_resources) {
        if (resource->getUrl() == url)
            return resource;
    }

    return QnResourcePtr(0);
}

QnNetworkResourcePtr QnResourcePool::getNetResourceByPhysicalId(const QString &physicalId) const
{
    QnMutexLocker locker( &m_resourcesMtx );
    for (const QnResourcePtr &resource: m_resources) {
        QnNetworkResourcePtr netResource = resource.dynamicCast<QnNetworkResource>();
        if (netResource != 0 && netResource->getPhysicalId() == physicalId)
            return netResource;
    }

    return QnNetworkResourcePtr(0);
}

QnResourcePtr QnResourcePool::getResourceByParam(const QString &key, const QString &value) const
{
    QnMutexLocker locker( &m_resourcesMtx );
    for (const QnResourcePtr &resource: m_resources)
    {
        if (resource->getProperty(key) == value)
            return resource;
    }

    return QnResourcePtr(0);
}

QnNetworkResourcePtr QnResourcePool::getResourceByMacAddress(const QString &mac) const
{
    QnMacAddress macAddress(mac);
    QnMutexLocker locker( &m_resourcesMtx );
    for (const QnResourcePtr &resource: m_resources) {
        QnNetworkResourcePtr netResource = resource.dynamicCast<QnNetworkResource>();
        if (netResource != 0 && netResource->getMAC() == macAddress)
            return netResource;
    }

    return QnNetworkResourcePtr(0);
}

QnVirtualCameraResourceList QnResourcePool::getAllCameras(const QnResourcePtr &mServer, bool ignoreDesktopCameras) const
{
    QnUuid parentId = mServer ? mServer->getId() : QnUuid();
    QnVirtualCameraResourceList result;
    QnMutexLocker locker( &m_resourcesMtx );
    for (const QnResourcePtr &resource: m_resources)
    {
        if (ignoreDesktopCameras && resource->hasFlags(Qn::desktop_camera))
            continue;

        QnVirtualCameraResourcePtr camResource = resource.dynamicCast<QnVirtualCameraResource>();
        if (camResource && (parentId.isNull() || camResource->getParentId() == parentId))
            result << camResource;
    }

    return result;
}

QnMediaServerResourceList QnResourcePool::getAllServers(Qn::ResourceStatus status) const
{
    QnMutexLocker lock( &m_resourcesMtx ); //m_resourcesMtx is recursive
    ensureCache();

    if (status == Qn::AnyStatus)
        return m_cache.serversList;

    const auto statusFilter = [status](const QnMediaServerResourcePtr &serverResource)
    {
        return (serverResource->getStatus() == status);
    };

    return m_cache.serversList.filtered(statusFilter);
}

QnResourceList QnResourcePool::getResourcesByParentId(const QnUuid& parentId) const
{
    QnResourceList result;
    QnMutexLocker locker( &m_resourcesMtx );
    for (const QnResourcePtr &resource: m_resources)
        if (resource->getParentId() == parentId)
            result << resource;

    return result;
}

QnResourceList QnResourcePool::getAllResourceByTypeName(const QString &typeName) const
{
    QnResourceList result;

    const QnResourceTypePtr resType = qnResTypePool->getResourceTypeByName(typeName);
    if (!resType)
        return result;

    QnMutexLocker locker( &m_resourcesMtx );
    for (const QnResourcePtr &resource: m_resources) {
        if (resource->getTypeId() == resType->getId())
            result << resource;
    }

    return result;
}

QnNetworkResourceList QnResourcePool::getAllNetResourceByPhysicalId(const QString &physicalId) const
{
    QnNetworkResourceList result;
    QnMutexLocker locker( &m_resourcesMtx );
    for (const QnResourcePtr &resource: m_resources) {
        QnNetworkResourcePtr netResource = resource.dynamicCast<QnNetworkResource>();
        if (netResource != 0 && netResource->getPhysicalId() == physicalId)
            result << netResource;
    }

    return result;
}

QnNetworkResourceList QnResourcePool::getAllNetResourceByHostAddress(const QHostAddress &hostAddress) const
{
    return getAllNetResourceByHostAddress(hostAddress.toString());
}

QnNetworkResourceList QnResourcePool::getAllNetResourceByHostAddress(const QString &hostAddress) const
{
    QnNetworkResourceList result;
    QnMutexLocker locker( &m_resourcesMtx );
    for (const QnResourcePtr &resource: m_resources) {
        QnNetworkResourcePtr netResource = resource.dynamicCast<QnNetworkResource>();
        if (netResource != 0 && netResource->getHostAddress() == hostAddress)
            result << netResource;
    }

    return result;
}

QnResourcePtr QnResourcePool::getResourceByUniqueId(const QString &uniqueID) const
{
    QnMutexLocker locker( &m_resourcesMtx );
    auto itr = std::find_if( m_resources.begin(), m_resources.end(), [&uniqueID](const QnResourcePtr &resource) { return resource->getUniqueId() == uniqueID; });
    return itr != m_resources.end() ? itr.value() : QnResourcePtr(0);
}

QnResourcePtr QnResourcePool::getResourceByDescriptor(const QnLayoutItemResourceDescriptor& descriptor) const
{
    QnResourcePtr result;
    if (!descriptor.id.isNull())
        result = getResourceById(descriptor.id);
    if (!result)
        result = getResourceByUniqueId(descriptor.uniqueId);
    return result;
}

void QnResourcePool::updateUniqId(const QnResourcePtr& res, const QString &newUniqId)
{
    QnMutexLocker locker( &m_resourcesMtx );
    res->setUniqId(newUniqId);
}

bool QnResourcePool::hasSuchResource(const QString &uniqid) const
{
    return !getResourceByUniqueId(uniqid).isNull();
}

QnResourceList QnResourcePool::getResourcesWithFlag(Qn::ResourceFlag flag) const
{
    QnResourceList result;

    QnMutexLocker locker( &m_resourcesMtx );
    for (const QnResourcePtr &resource: m_resources)
        if (resource->hasFlags(flag))
            result.append(resource);

    return result;
}

QnResourceList QnResourcePool::getResourcesWithParentId(QnUuid id) const
{
    QnMutexLocker locker( &m_resourcesMtx );

    // TODO: #Elric cache it, but remember that id and parentId of a resource may change
    // while it's in the pool.

    QnResourceList result;
    for(const QnResourcePtr &resource: m_resources)
        if(resource->getParentId() == id)
            result.push_back(resource);
    return result;
}

QnResourceList QnResourcePool::getResourcesWithTypeId(QnUuid id) const
{
    QnMutexLocker locker( &m_resourcesMtx );

    QnResourceList result;
    for(const QnResourcePtr &resource: m_resources)
        if(resource->getTypeId() == id)
            result.push_back(resource);
    return result;
}

QnUserResourcePtr QnResourcePool::getAdministrator() const
{
    QnMutexLocker locker( &m_resourcesMtx );
    if (m_adminResource)
        return m_adminResource;

    for(const QnResourcePtr &resource: m_resources)
    {
        QnUserResourcePtr user = resource.dynamicCast<QnUserResource>();
        if (user && user->isBuiltInAdmin())
        {
            m_adminResource = user;
            return user;
        }
    }
    return QnUserResourcePtr();
}

QStringList QnResourcePool::allTags() const
{
    QStringList result;

    QnMutexLocker locker( &m_resourcesMtx );
    for (const QnResourcePtr &resource: m_resources.values())
        result << resource->getTags();

    return result;
}

void QnResourcePool::clear()
{
    QnMutexLocker lk( &m_resourcesMtx );

    m_resources.clear();
}

bool QnResourcePool::containsIoModules() const {
    QnMutexLocker lk( &m_resourcesMtx );
    ensureCache();
    return m_cache.containsIoModules;
}

void QnResourcePool::markLayoutLiteClient(const QnLayoutResourcePtr& layout)
{
    if (!layout)
        return;

    layout->setProperty(kLiteClientLayoutKey, true);
}

QnLayoutResourceList QnResourcePool::getLayoutsWithResource(const QnUuid &cameraGuid) const {
    QnLayoutResourceList result;
    QnLayoutResourceList layouts = getResources<QnLayoutResource>();
    for (const auto &layout: layouts) {
        for (const QnLayoutItemData &item: layout->getItems()) {
            if (item.resource.id != cameraGuid)
                continue;
            result << layout;
            break;
        }
    }
    return result;
}

bool QnResourcePool::insertOrUpdateResource( const QnResourcePtr &resource, QHash<QnUuid, QnResourcePtr>* const resourcePool )
{
    const QnUuid& id = resource->getId();
    auto itr = resourcePool->find(id);
    if (itr == resourcePool->end()) {
        // new resource
        resourcePool->insert(id, resource);
        invalidateCache();
        return true;
    }
    else {
        // if we already have such resource in the pool
        itr.value()->update(resource);
        return false;
    }
}

QnResourcePtr QnResourcePool::getIncompatibleResourceById(const QnUuid &id, bool useCompatible) const {
    QnMutexLocker locker( &m_resourcesMtx );

    auto it = m_incompatibleResources.find(id);
    if (it != m_incompatibleResources.end())
        return it.value();

    if (useCompatible)
        return getResourceById(id);

    return QnResourcePtr();
}

QnResourceList QnResourcePool::getAllIncompatibleResources() const {
    QnMutexLocker locker( &m_resourcesMtx );
    return m_incompatibleResources.values();
}

QnVideoWallItemIndex QnResourcePool::getVideoWallItemByUuid(const QnUuid &uuid) const {
    QnMutexLocker lk( &m_resourcesMtx );
    for (const QnResourcePtr &resource: m_resources) {
        QnVideoWallResourcePtr videoWall = resource.dynamicCast<QnVideoWallResource>();
        if (!videoWall || !videoWall->items()->hasItem(uuid))
            continue;
        return QnVideoWallItemIndex(videoWall, uuid);
    }
    return QnVideoWallItemIndex();
}

QnVideoWallItemIndexList QnResourcePool::getVideoWallItemsByUuid(const QList<QnUuid> &uuids) const {
    QnVideoWallItemIndexList result;
    for (const QnUuid &uuid: uuids) {
        QnVideoWallItemIndex index = getVideoWallItemByUuid(uuid);
        if (!index.isNull())
            result << index;
    }
    return result;
}

QnVideoWallMatrixIndex QnResourcePool::getVideoWallMatrixByUuid(const QnUuid &uuid) const {
    QnMutexLocker lk( &m_resourcesMtx );
    for (const QnResourcePtr &resource: m_resources) {
        QnVideoWallResourcePtr videoWall = resource.dynamicCast<QnVideoWallResource>();
        if (!videoWall || !videoWall->matrices()->hasItem(uuid))
            continue;
        return QnVideoWallMatrixIndex(videoWall, uuid);
    }
    return QnVideoWallMatrixIndex();
}


QnVideoWallMatrixIndexList QnResourcePool::getVideoWallMatricesByUuid(const QList<QnUuid> &uuids) const {
    QnVideoWallMatrixIndexList result;
    for (const QnUuid &uuid: uuids) {
        QnVideoWallMatrixIndex index = getVideoWallMatrixByUuid(uuid);
        if (!index.isNull())
            result << index;
    }
    return result;
}

void QnResourcePool::invalidateCache()
{
    QnMutexLocker lk( &m_resourcesMtx );
    m_cache.valid = false;
    m_cache.containsIoModules = false;
    m_cache.serversList.clear();
}

void QnResourcePool::ensureCache() const {
    QnMutexLocker lk( &m_resourcesMtx );
    if (m_cache.valid)
        return;

    m_cache.containsIoModules = boost::algorithm::any_of(getResources<QnVirtualCameraResource>(), [](const QnVirtualCameraResourcePtr &device) {
        return device->isIOModule() && !device->hasVideo(nullptr);
    });
    m_cache.serversList = getResources<QnMediaServerResource>();
    m_cache.valid = true;
}
