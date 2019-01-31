#include "resource_pool.h"

#include <core/resource/media_server_resource.h>
#include <core/resource/network_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/videowall_item.h>
#include <core/resource/videowall_item_index.h>
#include <core/resource/videowall_matrix_index.h>

#include <utils/common/checked_cast.h>

#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <core/resource_access/resource_access_filter.h>
#include <api/helpers/camera_id_helper.h>

namespace {

// Returns true, if a resource has been inserted. false - if updated existing resource
template<class T>
bool insertOrUpdateResource(const T& resource, QHash<QnUuid, T>* const resourcePool)
{
    QnUuid id = resource->getId();
    auto itr = resourcePool->find(id);
    if (itr == resourcePool->end())
    {
        // Inserting new resource.
        resourcePool->insert(id, resource);
        return true;
    }

    // Updating existing resource.
    itr.value()->update(resource);
    return false;
}

} // namespace

static const QString kLiteClientLayoutKey = lit("liteClient");

QnResourcePool::QnResourcePool(QObject* parent):
    base_type(parent),
    QnCommonModuleAware(parent),
    m_resourcesMtx(QnMutex::Recursive),
    m_tranInProgress(false)
{
    m_threadPool.reset(new QThreadPool());
}

QnResourcePool::~QnResourcePool()
{
    QnMutexLocker locker(&m_resourcesMtx);
    m_adminResource.clear();
    m_resources.clear();
}

 QThreadPool* QnResourcePool::threadPool() const
{
    return m_threadPool.get();
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

void QnResourcePool::addResource(const QnResourcePtr& resource, AddResourceFlags flags)
{
    if (!flags.testFlag(SkipAddingTransaction) && m_tranInProgress)
        m_tmpResources << resource;
    else
        addResources({{resource}}, flags);
}

void QnResourcePool::addIncompatibleServer(const QnMediaServerResourcePtr& server)
{
    addResources({{server}}, UseIncompatibleServerPool);
}

void QnResourcePool::addNewResources(const QnResourceList& resources, AddResourceFlags flags)
{
    addResources(
        resources.filtered(
            [](const QnResourcePtr& resource) { return resource->resourcePool() == nullptr; }));
}

void QnResourcePool::addResources(const QnResourceList& resources, AddResourceFlags flags)
{
    QnMutexLocker resourcesLock(&m_resourcesMtx);

    for (const auto& resource: resources)
    {
        // Getting an NX_ASSERT here? Did you forget to use QnSharedResourcePointer?
        NX_ASSERT(resource->toSharedPointer());
        NX_ASSERT(!resource->getId().isNull());
        NX_ASSERT(!resource->resourcePool() || resource->resourcePool() == this);
        resource->setResourcePool(this);
        resource->moveToThread(thread());
    }

    QMap<QnUuid, QnResourcePtr> newResources; // sort by id

    for (const QnResourcePtr& resource: resources)
    {
        NX_ASSERT(!resource->getId().isNull(), "Got resource with empty id.");
        if (resource->getId().isNull())
            continue;

        if (!flags.testFlag(UseIncompatibleServerPool))
        {
            if (insertOrUpdateResource(resource, &m_resources))
            {
                m_cache.resourceAdded(resource);
                newResources.insert(resource->getId(), resource);
            }
            m_incompatibleServers.remove(resource->getId());
        }
        else
        {
            auto server = resource.dynamicCast<QnMediaServerResource>();
            NX_ASSERT(server, "Only fake servers allowed here");
            if (insertOrUpdateResource(server, &m_incompatibleServers))
                newResources.insert(resource->getId(), resource);
        }
    }

    resourcesLock.unlock();

    QnResourceList addedResources = newResources.values();
    for (const auto& resource: addedResources)
    {
        connect(resource, &QnResource::statusChanged, this, &QnResourcePool::statusChanged);
        connect(resource, &QnResource::statusChanged, this, &QnResourcePool::resourceChanged);
        connect(resource, &QnResource::resourceChanged, this, &QnResourcePool::resourceChanged);

        if (!resource->hasFlags(Qn::foreigner))
        {
            if (resource->getStatus() != Qn::Offline)
                resource->initAsync(false);
        }
    }

    for (const auto& resource: addedResources)
    {
        NX_VERBOSE(this) << "RESOURCE ADDED"
            << resource->metaObject()->className()
            << resource->getName();
        emit resourceAdded(resource);
    }
}

void QnResourcePool::removeResources(const QnResourceList& resources)
{
    QnResourceList removedLayoutResources;
    QnResourceList removedUsers;
    QnResourceList removedOtherResources;
    auto appendRemovedResource =
        [&](QnResourcePtr resource)
        {
            if (resource->hasFlags(Qn::layout))
                removedLayoutResources.push_back(resource);
            else if (resource->hasFlags(Qn::user))
                removedUsers.push_back(resource);
            else
                removedOtherResources.push_back(resource);
        };

    QnMutexLocker lk(&m_resourcesMtx);

    for (const QnResourcePtr& resource: resources)
    {
        if (!resource || resource->resourcePool() != this)
            continue;

        resource->disconnect(this);
        resource->addFlags(Qn::removed);

        //have to remove by id, since uniqueId can be MAC and, as a result, not unique among friend and foreign resources
        QnUuid resId = resource->getId();
        if (m_adminResource && resId == m_adminResource->getId())
            m_adminResource.clear();

        const auto resIter = m_resources.find(resId);
        if (resIter != m_resources.end())
        {
            m_cache.resourceRemoved(resource);
            m_resources.erase(resIter);
            appendRemovedResource(resource);
        }
        else
        {
            const auto iter = m_incompatibleServers.find(resource->getId());
            if (iter != m_incompatibleServers.end())
            {
                m_incompatibleServers.erase(iter);
                appendRemovedResource(resource);
            }
        }

        resource->setResourcePool(nullptr);
    }

    /* Remove layout resources. */
    const auto videoWalls = getResources<QnVideoWallResource>();
    for (const QnResourcePtr& layoutResource: removedLayoutResources)
    {
        for (const QnVideoWallResourcePtr& videowall: videoWalls)
            // TODO: #Elric this is way beyond what one may call 'suboptimal'.
        {
            for (QnVideoWallItem item: videowall->items()->getItems())
            {
                if (item.layout != layoutResource->getId())
                    continue;
                item.layout = QnUuid();
                videowall->items()->updateItem(item);
            }
        }

        NX_VERBOSE(this) << "RESOURCE REMOVED"
            << layoutResource->metaObject()->className()
            << layoutResource->getName();
    }

    /* Remove other resources. */
    const auto layouts = getResources<QnLayoutResource>();
    for (const QnResourcePtr& otherResource: removedOtherResources)
    {
        for (const QnLayoutResourcePtr& layoutResource: layouts)
            // TODO: #Elric this is way beyond what one may call 'suboptimal'.
            for (const QnLayoutItemData& data: layoutResource->getItems())
                if (data.resource.id == otherResource->getId() || data.resource.uniqueId ==
                    otherResource->getUniqueId())
                    layoutResource->removeItem(data);

        NX_VERBOSE(this) << "RESOURCE REMOVED"
            << otherResource->metaObject()->className()
            << otherResource->getName();
    }

    lk.unlock();

    /* Emit notifications. */
    for (auto layoutResource: removedLayoutResources)
        emit resourceRemoved(layoutResource);

    for (auto resource: removedOtherResources)
        emit resourceRemoved(resource);

    for (auto user: removedUsers)
        emit resourceRemoved(user);
}

QnResourceList QnResourcePool::getResources() const
{
    QnMutexLocker locker(&m_resourcesMtx);
    return m_resources.values();
}

QnResourcePtr QnResourcePool::getResourceById(const QnUuid& id) const
{
    QnMutexLocker locker(&m_resourcesMtx);
    auto resIter = m_resources.find(id);
    if (resIter != m_resources.end())
        return resIter.value();
    return QnResourcePtr();
}

QnSecurityCamResourceList QnResourcePool::getResourcesBySharedId(const QString& sharedId) const
{
    return getResources<QnSecurityCamResource>(
        [&sharedId](const QnSecurityCamResourcePtr& camera)
        {
            return camera->getSharedId() == sharedId;
        });
}

QnResourceList QnResourcePool::getResourcesByLogicalId(int value) const
{
    if (value <= 0)
        return QnResourceList();
    return getResources([=](const auto& resource) { return resource->logicalId() == value; });
}

QnResourcePtr QnResourcePool::getResourceByUrl(const QString& url) const
{
    return getResource([&url](const QnResourcePtr& resource){return resource->getUrl() == url; });
}

QnNetworkResourcePtr QnResourcePool::getNetResourceByPhysicalId(const QString& physicalId) const
{
    return getResource<QnNetworkResource>(
        [&physicalId](const QnNetworkResourcePtr& resource)
        {
            return resource->getPhysicalId() == physicalId;
        });
}

QnNetworkResourcePtr QnResourcePool::getResourceByMacAddress(const QString& mac) const
{
    nx::utils::MacAddress macAddress(mac);
    if (macAddress.isNull())
        return {};

    return getResource<QnNetworkResource>(
        [&macAddress](const QnNetworkResourcePtr& resource)
        {
            return resource->getMAC() == macAddress;
        });
}

QnVirtualCameraResourceList QnResourcePool::getAllCameras(
    const QnResourcePtr& mServer,
    bool ignoreDesktopCameras) const
{
    QnUuid parentId = mServer ? mServer->getId() : QnUuid();
    QnVirtualCameraResourceList result;
    QnMutexLocker locker(&m_resourcesMtx);
    for (const QnResourcePtr& resource: m_resources)
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
    QnMutexLocker lock(&m_resourcesMtx); //m_resourcesMtx is recursive

    if (status == Qn::AnyStatus)
        return m_cache.mediaServers.values();

    QnMediaServerResourceList result;
    for (const auto& server: m_cache.mediaServers.values())
    {
        if (server->getStatus() == status)
            result.push_back(server);
    }
    return result;
}

QnResourceList QnResourcePool::getResourcesByParentId(const QnUuid& parentId) const
{
    return getResources(
        [&parentId](const QnResourcePtr& resource)
        {
            return resource->getParentId() == parentId;
        });
}

QnNetworkResourceList QnResourcePool::getAllNetResourceByHostAddress(
    const QString& hostAddress) const
{
    return getResources<QnNetworkResource>(
        [&hostAddress](const QnNetworkResourcePtr& resource)
        {
            return resource->getHostAddress() == hostAddress;
        });
}

QnResourcePtr QnResourcePool::getResourceByUniqueId(const QString& uniqueId) const
{
    QnMutexLocker locker(&m_resourcesMtx);
    return m_cache.resourcesByUniqueId.value(uniqueId);
}

QnResourcePtr QnResourcePool::getResourceByDescriptor(
    const QnLayoutItemResourceDescriptor& descriptor) const
{
    if (!descriptor.id.isNull())
    {
        if (const auto result = getResourceById(descriptor.id))
            return result;
    }

    if (descriptor.uniqueId.isEmpty())
        return QnResourcePtr();

    if (const auto result = getResourceByUniqueId(descriptor.uniqueId))
        return result;

    auto openableInLayout =
        getResourcesByLogicalId(descriptor.uniqueId.toInt()).filtered(
            QnResourceAccessFilter::isShareableMedia);

    return openableInLayout.empty() ? QnResourcePtr() : openableInLayout.first();
}

QnResourceList QnResourcePool::getResourcesWithFlag(Qn::ResourceFlag flag) const
{
    return getResources([&flag](const QnResourcePtr& resource){return resource->hasFlags(flag);});
}

QnUserResourcePtr QnResourcePool::getAdministrator() const
{
    QnMutexLocker locker(&m_resourcesMtx);
    if (m_adminResource)
        return m_adminResource;

    for (const QnResourcePtr& resource: m_resources)
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

void QnResourcePool::clear()
{
    QnMutexLocker lk(&m_resourcesMtx);
    m_resources.clear();
}

bool QnResourcePool::containsIoModules() const
{
    QnMutexLocker lk(&m_resourcesMtx);
    return m_cache.ioModulesCount > 0;
}

void QnResourcePool::markLayoutLiteClient(const QnLayoutResourcePtr& layout)
{
    NX_ASSERT(layout);
    if (layout)
        layout->setProperty(kLiteClientLayoutKey, true);
}

QnMediaServerResourcePtr QnResourcePool::getIncompatibleServerById(
    const QnUuid& id,
    bool useCompatible) const
{
    QnMutexLocker locker(&m_resourcesMtx);

    auto it = m_incompatibleServers.find(id);
    if (it != m_incompatibleServers.end())
        return it.value();

    if (useCompatible)
        return getResourceById(id).dynamicCast<QnMediaServerResource>();

    return QnMediaServerResourcePtr();
}

QnMediaServerResourceList QnResourcePool::getIncompatibleServers() const
{
    QnMutexLocker locker(&m_resourcesMtx);
    return m_incompatibleServers.values();
}

QnVideoWallItemIndex QnResourcePool::getVideoWallItemByUuid(const QnUuid& uuid) const
{
    QnMutexLocker lk(&m_resourcesMtx);
    for (const QnResourcePtr& resource: m_resources)
    {
        QnVideoWallResourcePtr videoWall = resource.dynamicCast<QnVideoWallResource>();
        if (!videoWall || !videoWall->items()->hasItem(uuid))
            continue;
        return QnVideoWallItemIndex(videoWall, uuid);
    }
    return QnVideoWallItemIndex();
}

QnVideoWallItemIndexList QnResourcePool::getVideoWallItemsByUuid(const QList<QnUuid>& uuids) const
{
    QnVideoWallItemIndexList result;
    for (const QnUuid& uuid: uuids)
    {
        QnVideoWallItemIndex index = getVideoWallItemByUuid(uuid);
        if (!index.isNull())
            result << index;
    }
    return result;
}

QnVideoWallMatrixIndex QnResourcePool::getVideoWallMatrixByUuid(const QnUuid& uuid) const
{
    QnMutexLocker lk(&m_resourcesMtx);
    for (const QnResourcePtr& resource: m_resources)
    {
        QnVideoWallResourcePtr videoWall = resource.dynamicCast<QnVideoWallResource>();
        if (!videoWall || !videoWall->matrices()->hasItem(uuid))
            continue;
        return QnVideoWallMatrixIndex(videoWall, uuid);
    }
    return QnVideoWallMatrixIndex();
}

QnVideoWallMatrixIndexList QnResourcePool::getVideoWallMatricesByUuid(
    const QList<QnUuid>& uuids) const
{
    QnVideoWallMatrixIndexList result;
    for (const QnUuid& uuid: uuids)
    {
        QnVideoWallMatrixIndex index = getVideoWallMatrixByUuid(uuid);
        if (!index.isNull())
            result << index;
    }
    return result;
}

bool QnResourcePool::Cache::isIoModule(const QnResourcePtr& res) const
{
    if (const auto& device = res.dynamicCast<QnVirtualCameraResource>())
        return device->isIOModule() && !device->hasVideo(nullptr);
    return false;
}

void QnResourcePool::Cache::resourceRemoved(const QnResourcePtr& res)
{
    resourcesByUniqueId.remove(res->getUniqueId());
    mediaServers.remove(res->getId());
    if (isIoModule(res))
        --ioModulesCount;
}

void QnResourcePool::Cache::resourceAdded(const QnResourcePtr& res)
{
    resourcesByUniqueId.insert(res->getUniqueId(), res);
    if (const auto& server = res.dynamicCast<QnMediaServerResource>())
        mediaServers.insert(server->getId(), server);
    else if (isIoModule(res))
        ++ioModulesCount;
}

QnVirtualCameraResourceList QnResourcePool::getCamerasByFlexibleIds(
    const std::vector<QString>& flexibleIdList) const
{
    QnVirtualCameraResourceList result;
    for (const auto& flexibleId: flexibleIdList)
    {
        if (auto camera = nx::camera_id_helper::findCameraByFlexibleId(this, flexibleId))
            result << camera;
    }

    return result;
}
