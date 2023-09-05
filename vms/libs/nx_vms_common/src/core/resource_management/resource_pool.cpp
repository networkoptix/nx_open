// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_pool.h"

#include <QtCore/QThreadPool>

#include <api/helpers/camera_id_helper.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/network_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_item.h>
#include <core/resource/videowall_item_index.h>
#include <core/resource/videowall_matrix_index.h>
#include <core/resource/videowall_resource.h>
#include <core/resource_access/resource_access_filter.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/vms/common/system_context.h>
#include <utils/common/checked_cast.h>

#include "private/resource_pool_p.h"

namespace {

// Returns updates function if resource exists and requires an update.
template<class T>
std::function<void()> insertOrUpdateResource(const T& resource, QHash<QnUuid, T>* const resourcePool)
{
    QnUuid id = resource->getId();
    auto itr = resourcePool->constFind(id);
    if (itr == resourcePool->cend())
    {
        resourcePool->insert(id, resource);
        return nullptr;
    }

    return [existing = itr.value(), resource] { existing->update(resource); };
}

} // namespace

QnResourcePool::QnResourcePool(nx::vms::common::SystemContext* systemContext, QObject* parent):
    base_type(parent),
    d(new Private(this, systemContext)),
    m_resourcesMutex(nx::ReadWriteLock::Recursive),
    m_tranInProgress(false)
{
    m_threadPool.reset(new QThreadPool());
    NX_DEBUG(this, "Created");
}

QnResourcePool::~QnResourcePool()
{
    clear();
    NX_DEBUG(this, "Removing");
}

nx::vms::common::SystemContext* QnResourcePool::systemContext() const
{
    return d->systemContext;
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
        m_tmpResources.append(resource);
    else
        addResources({{resource}}, flags);
}

void QnResourcePool::addIncompatibleServer(const QnMediaServerResourcePtr& server)
{
    addResources({{server}}, UseIncompatibleServerPool);
}

void QnResourcePool::addNewResources(const QnResourceList& resources, AddResourceFlags /*flags*/)
{
    addResources(
        resources.filtered(
            [](const QnResourcePtr& resource) { return resource->resourcePool() == nullptr; }));
}

void QnResourcePool::addResources(const QnResourceList& resources, AddResourceFlags flags)
{
    for (const auto& resource: resources)
    {
        // Getting an NX_ASSERT here? Did you forget to use QnSharedResourcePointer?
        NX_ASSERT(resource->toSharedPointer());
        NX_ASSERT(!resource->getId().isNull());
        NX_ASSERT(!resource->systemContext());

        resource->addToSystemContext(d->systemContext);
        resource->moveToThread(thread());
    }

    NX_WRITE_LOCKER resourcesLock(&m_resourcesMutex);

    QMap<QnUuid, QnResourcePtr> newResources; // Sort by id.
    std::vector<std::function<void()>> updateExistingResources;
    for (const QnResourcePtr& resource: resources)
    {
        NX_ASSERT(!resource->getId().isNull(), "Got resource with empty id.");
        if (resource->getId().isNull())
            continue;

        if (!flags.testFlag(UseIncompatibleServerPool))
        {
            if (auto updateExisting = insertOrUpdateResource(resource, &m_resources))
            {
                updateExistingResources.push_back(std::move(updateExisting));
            }
            else
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
            if (auto updateExisting = insertOrUpdateResource(server, &m_incompatibleServers))
                updateExistingResources.push_back(std::move(updateExisting));
            else
                newResources.insert(resource->getId(), resource);
        }
    }

    resourcesLock.unlock();

    // Resource updates may emit signal, which should not be done under mutex.
    for (const auto& update: updateExistingResources)
        update();

    QnResourceList addedResources;
    addedResources.reserve(newResources.size());
    for (const auto& resource: std::as_const(newResources))
        addedResources.push_back(resource);

    for (const auto& resource: std::as_const(addedResources))
    {
        connect(resource.get(), &QnResource::statusChanged, this, &QnResourcePool::statusChanged);
        connect(resource.get(), &QnResource::statusChanged, this, &QnResourcePool::resourceChanged);
        connect(resource.get(), &QnResource::resourceChanged, this, &QnResourcePool::resourceChanged);
    }

    for (const auto& resource: std::as_const(addedResources))
    {
        NX_VERBOSE(this, "Added resource %1, id: %2, name: %3",
            resource, resource->getId(), resource->getName());
        emit resourceAdded(resource);
    }

    if (!addedResources.empty())
        emit resourcesAdded(addedResources);
}

void QnResourcePool::removeResources(const QnResourceList& resources)
{
    if (resources.empty())
        return;

    QVector<QnResourcePtr> removedLayouts;
    QVector<QnResourcePtr> removedUsers;
    QVector<QnResourcePtr> removedOtherResources;
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

    QnResourceList removedResources;
    for (const QnResourcePtr& resource: std::as_const(resources))
    {
        if (!resource || resource->hasFlags(Qn::removed) || resource->resourcePool() != this)
        {
            NX_VERBOSE(this, "Skip different pool resource %1, id: %2, name: %3",
                resource, resource->getId(), resource->getName());
            continue;
        }

        NX_VERBOSE(this, "Removing resource %1, id: %2, name: %3",
            resource, resource->getId(), resource->getName());

        resource->disconnect(this);
        resource->addFlags(Qn::removed);
        removedResources.append(resource);
    }

    NX_WRITE_LOCKER lk(&m_resourcesMutex);
    for (const QnResourcePtr& resource: removedResources)
    {
        // Have to remove by id, since physicalId can be MAC and, as a result, not unique among
        // friend and foreign resources.
        const QnUuid resId = resource->getId();
        if (m_adminResource && resId == m_adminResource->getId())
            m_adminResource.clear();

        const auto resIter = m_resources.constFind(resId);
        if (resIter != m_resources.cend())
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

    const bool onlyUsers = removedUsers.size() == removedResources.size();

    // After resources removing, we must check if removed layouts left on the videowall items
    // and if the removed cameras / web pages / server left as the layout items.
    // #vbreus Is lookup by flag suitable there?
    const auto existingVideoWalls = onlyUsers
        ? QnSharedResourcePointerList<QnVideoWallResource>()
        : getResourcesUnsafe<QnVideoWallResource>();
    const auto existingLayouts = onlyUsers
        ? QnSharedResourcePointerList<QnLayoutResource>()
        : getResourcesUnsafe<QnLayoutResource>();

    lk.unlock();

    // Remove layout resources from the videowalls
    QSet<QnUuid> removedLayoutsIds;
    for (const auto& layout: std::as_const(removedLayouts))
        removedLayoutsIds.insert(layout->getId());

    for (const auto& videowall: existingVideoWalls)
    {
        const auto videowallItems = videowall->items()->getItems();
        for (const QnVideoWallItem& item: videowallItems)
        {
            if (removedLayoutsIds.contains(item.layout))
            {
                QnVideoWallItem updatedItem(item);
                updatedItem.layout = QnUuid();
                videowall->items()->updateItem(updatedItem);
            }
        }
    }

    // Remove other resources from layouts.
    QSet<QnUuid> removedResourcesIds;
    QSet<QString> removedResourcesPhysicalIds;
    for (const auto& resource: std::as_const(removedOtherResources))
    {
        removedResourcesIds.insert(resource->getId());
        if (auto networkResource = resource.dynamicCast<QnNetworkResource>())
            removedResourcesPhysicalIds.insert(networkResource->getPhysicalId());
    }

    for (const auto& layout: existingLayouts)
    {
        // TODO: #sivanov Search layout item by universal id is a bit more complex. Unify the process.
        const auto layoutItems = layout->getItems();
        for (const auto& item: layoutItems)
        {
            if (removedResourcesIds.contains(item.resource.id)
                || removedResourcesPhysicalIds.contains(item.resource.path))
            {
                layout->removeItem(item);
            }
        }
    }

    // Emit notifications.
    for (const auto& layout: std::as_const(removedLayouts))
    {
        NX_DEBUG(this, "Remove layout resource %1", layout);
        emit resourceRemoved(layout);
    }

    for (const auto& resource: std::as_const(removedOtherResources))
    {
        NX_DEBUG(this, "Remove other resource %1", resource);
        emit resourceRemoved(resource);
    }

    for (const auto& user: std::as_const(removedUsers))
    {
        NX_DEBUG(this, "Remove user resource %1", user);
        emit resourceRemoved(user);
    }

    if (!removedResources.empty())
        emit resourcesRemoved(removedResources);
}

QnResourceList QnResourcePool::getResources() const
{
    NX_READ_LOCKER locker(&m_resourcesMutex);
    return m_resources.values();
}

QnResourcePtr QnResourcePool::getResourceById(const QnUuid& id) const
{
    NX_READ_LOCKER locker(&m_resourcesMutex);
    return m_resources.value(id, QnResourcePtr());
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

QnResourcePtr QnResourcePool::getResourceByUrl(
    const QString& url, const QnUuid& parentServerId) const
{
    return getResource(
        [&url, &parentServerId](const QnResourcePtr& resource)
        {
            if (!parentServerId.isNull() && resource->getParentId() != parentServerId)
                return false;
            return resource->getUrl() == url;
        });
}

QnNetworkResourcePtr QnResourcePool::getNetworkResourceByPhysicalId(const QString& physicalId) const
{
    NX_READ_LOCKER locker(&m_resourcesMutex);
    return d->resourcesByPhysicalId.value(physicalId);
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
    const QnUuid& parentId, bool ignoreDesktopCameras) const
{
    QnVirtualCameraResourceList result;
    NX_READ_LOCKER locker(&m_resourcesMutex);
    for (const QnResourcePtr& resource: m_resources)
    {
        if (ignoreDesktopCameras && resource->hasFlags(Qn::desktop_camera))
            continue;

        QnVirtualCameraResourcePtr camResource = resource.dynamicCast<QnVirtualCameraResource>();
        if (camResource && (parentId.isNull() || camResource->getParentId() == parentId))
            result.append(camResource);
    }

    return result;
}

QnVirtualCameraResourceList QnResourcePool::getAllCameras(
    const QnResourcePtr& server, bool ignoreDesktopCameras) const
{
    return getAllCameras(server ? server->getId() : QnUuid(), ignoreDesktopCameras);
}

QnMediaServerResourcePtr QnResourcePool::serverWithInternetAccess() const
{
    for (const auto server: this->servers())
    {
        if (server->hasInternetAccess())
            return server;
    }
    return {}; //< No internet access found.
}

QnMediaServerResourcePtr QnResourcePool::masterCloudSyncServer() const
{
    NX_READ_LOCKER lock(&m_resourcesMutex);
    for (const auto& server: std::as_const(d->mediaServers))
    {
        if (server->isMasterCloudSync())
            return server;
    }
    return {};
}

QnMediaServerResourceList QnResourcePool::servers() const
{
    NX_READ_LOCKER lock(&m_resourcesMutex);
    return d->mediaServers.values();
}

QnStorageResourceList QnResourcePool::storages() const
{
    NX_READ_LOCKER lock(&m_resourcesMutex);
    return d->storages.values();
}

QnMediaServerResourceList QnResourcePool::getAllServers(nx::vms::api::ResourceStatus status) const
{
    NX_READ_LOCKER lock(&m_resourcesMutex);
    QnMediaServerResourceList result;
    for (const auto& server: std::as_const(d->mediaServers))
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
    const nx::String& hostAddress) const
{
    return getResources<QnNetworkResource>(
        [&hostAddress](const QnNetworkResourcePtr& resource)
        {
            return resource->getHostAddress() == hostAddress;
        });
}

QnResourcePtr QnResourcePool::getResourceByDescriptor(
    const nx::vms::common::ResourceDescriptor& descriptor) const
{
    if (!descriptor.id.isNull())
    {
        if (const auto result = getResourceById(descriptor.id))
            return result;
    }

    if (descriptor.path.isEmpty())
        return QnResourcePtr();

    auto openableInLayout =
        getResourcesByLogicalId(descriptor.path.toInt()).filtered(
            QnResourceAccessFilter::isShareableMedia);

    return openableInLayout.empty() ? QnResourcePtr() : openableInLayout.first();
}

QnResourceList QnResourcePool::getResourcesWithFlag(Qn::ResourceFlag flag) const
{
    return getResources([&flag](const QnResourcePtr& resource){return resource->hasFlags(flag);});
}

QnUserResourcePtr QnResourcePool::getAdministrator() const
{
    NX_WRITE_LOCKER locker(&m_resourcesMutex);
    if (m_adminResource)
        return m_adminResource;

    auto itr = m_resources.find(QnUserResource::kAdminGuid);
    if (itr != m_resources.end())
        m_adminResource = itr.value().dynamicCast<QnUserResource>();

    return m_adminResource;
}

std::pair<QnUserResourcePtr, bool> QnResourcePool::userByName(const QString& name) const
{
    NX_READ_LOCKER locker(&m_resourcesMutex);
    const auto it = d->usersByName.find(name.toLower());
    if (it == d->usersByName.end())
        return {};
    return it->second.main();
}

void QnResourcePool::clear()
{
    QVector<QnResourcePtr> tempList;
    {
        NX_WRITE_LOCKER lk(&m_resourcesMutex);

        for (const auto& resource: std::as_const(m_resources))
            tempList.append(resource);
        for (const auto& resource: std::as_const(m_tmpResources))
            tempList.append(resource);
        for (const auto& resource: std::as_const(m_incompatibleServers))
            tempList.append(resource);

        m_tmpResources.clear();
        m_resources.clear();
        m_incompatibleServers.clear();
        m_adminResource.clear();

        d->ioModules.clear();
        d->hasIoModules = false;
        d->mediaServers.clear();
        d->storages.clear();
        d->resourcesByPhysicalId.clear();
        d->usersByName.clear();
    }

    NX_VERBOSE(this, "Cleared resources: %1", nx::containerString(tempList));

    for (const auto& resource: std::as_const(tempList))
    {
        resource->addFlags(Qn::removed);
        resource->disconnect(this);
    }
}

bool QnResourcePool::containsIoModules() const
{
    return d->hasIoModules;
}

QnMediaServerResourcePtr QnResourcePool::getIncompatibleServerById(
    const QnUuid& id,
    bool useCompatible) const
{
    NX_READ_LOCKER locker(&m_resourcesMutex);

    auto it = m_incompatibleServers.constFind(id);
    if (it != m_incompatibleServers.cend())
        return it.value();

    if (useCompatible)
    {
        locker.unlock();
        return getResourceById<QnMediaServerResource>(id);
    }

    return QnMediaServerResourcePtr();
}

QnMediaServerResourceList QnResourcePool::getIncompatibleServers() const
{
    NX_READ_LOCKER locker(&m_resourcesMutex);
    return m_incompatibleServers.values();
}

QnVideoWallItemIndex QnResourcePool::getVideoWallItemByUuid(const QnUuid& uuid) const
{
    NX_READ_LOCKER lk(&m_resourcesMutex);
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
            result.append(index);
    }
    return result;
}

QnVideoWallMatrixIndex QnResourcePool::getVideoWallMatrixByUuid(const QnUuid& uuid) const
{
    NX_READ_LOCKER lk(&m_resourcesMutex);
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
            result.append(index);
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
            result.append(camera);
    }

    return result;
}
