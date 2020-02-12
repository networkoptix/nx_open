#include "resource_pool.h"

#include <QtCore/QThreadPool>

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

#include "private/resource_pool_p.h"
#include <common/common_module.h>

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
    d(new Private(this)),
    m_resourcesMtx(QnMutex::Recursive),
    m_tranInProgress(false)
{
    m_threadPool.reset(new QThreadPool());
}

QnResourcePool::~QnResourcePool()
{
    QnMutexLocker locker(&m_resourcesMtx);
    clear();
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
                d->handleResourceAdded(resource);
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
        NX_VERBOSE(this, "Added resource %1: %2", resource, resource->getName());
        emit resourceAdded(resource);
    }
}

void QnResourcePool::removeResources(const QnResourceList& resources)
{
    NX_VERBOSE(this, "Removing resources %1", resources);

    QnResourceList removedLayouts;
    QnResourceList removedUsers;
    QnResourceList removedOtherResources;
    auto appendRemovedResource =
        [&](const QnResourcePtr& resource)
        {
            if (resource->hasFlags(Qn::layout))
                removedLayouts.push_back(resource);
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
            d->handleResourceRemoved(resource);
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
    }

    // After resources removing, we must check if removed layouts left on the videowall items and
    // if the removed cameras / webpages / server left as the layout items.
    const auto existingVideoWalls = getResources<QnVideoWallResource>();
    const auto existingLayouts = getResources<QnLayoutResource>();

    lk.unlock();

    // Remove layout resources from the videowalls
    QSet<QnUuid> removedLayoutsIds;
    for (const auto& layout: removedLayouts)
        removedLayoutsIds.insert(layout->getId());

    for (const auto& videowall: existingVideoWalls)
    {
        for (QnVideoWallItem item: videowall->items()->getItems())
        {
            if (removedLayoutsIds.contains(item.layout))
            {
                item.layout = QnUuid();
                videowall->items()->updateItem(item);
            }
        }
    }

    // Remove other resources from layouts.
    QSet<QnUuid> removedResourcesIds;
    QSet<QString> removedResourcesUniqueIds;
    for (const auto& resource: removedOtherResources)
    {
        removedResourcesIds.insert(resource->getId());
        removedResourcesUniqueIds.insert(resource->getUniqueId());
    }

    for (const auto& layout: existingLayouts)
    {
        // TODO: #GDM Search layout item by universal id is a bit more complex. Unify the process.
        for (const auto& item: layout->getItems())
        {
            if (removedResourcesIds.contains(item.resource.id)
                || removedResourcesUniqueIds.contains(item.resource.uniqueId))
            {
                layout->removeItem(item);
            }
        }
    }

    /* Emit notifications. */
    for (auto layout: removedLayouts)
        emit resourceRemoved(layout);

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

QnMediaServerResourcePtr QnResourcePool::getOwnMediaServer() const
{
    return getResourceById<QnMediaServerResource>(commonModule()->remoteGUID());
}

QnMediaServerResourcePtr QnResourcePool::getOwnMediaServerOrThrow() const
{
    const auto server = getOwnMediaServer();
    if (!server)
        throw std::logic_error("Failed to find own resource entity");
    return server;
}

QnMediaServerResourceList QnResourcePool::getAllServers(Qn::ResourceStatus status) const
{
    QnMutexLocker lock(&m_resourcesMtx); //m_resourcesMtx is recursive

    if (status == Qn::AnyStatus)
        return d->mediaServers.values();

    QnMediaServerResourceList result;
    for (const auto& server: d->mediaServers)
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
    return d->resourcesByUniqueId.value(uniqueId);
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
    QnResourceList tempList;
    {
        QnMutexLocker lk(&m_resourcesMtx);

        for (const auto& resource: m_resources)
            tempList << resource;
        for (const auto& resource: m_tmpResources)
            tempList << resource;
        for (const auto& resource: m_incompatibleServers)
            tempList << resource;

        m_tmpResources.clear();
        m_resources.clear();
        m_incompatibleServers.clear();
        m_adminResource.clear();

        d->ioModules.clear();
        d->mediaServers.clear();
        d->resourcesByUniqueId.clear();
    }

    for (const auto& resource: tempList)
        resource->disconnect(this);
}

bool QnResourcePool::containsIoModules() const
{
    QnMutexLocker lk(&m_resourcesMtx);
    return !d->ioModules.empty();
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
