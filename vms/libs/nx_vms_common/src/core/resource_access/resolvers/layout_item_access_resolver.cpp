// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_item_access_resolver.h"

#include <QtCore/QPointer>

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/utils/range_adapters.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/common/utils/layout_item_watcher.h>
#include <nx/vms/common/utils/videowall_layout_watcher.h>

namespace nx::core::access {

using namespace nx::vms::api;
using namespace nx::vms::common;

class LayoutItemAccessResolver::Private: public QObject
{
    LayoutItemAccessResolver* const q;

public:
    explicit Private(LayoutItemAccessResolver* q, AbstractResourceAccessResolver* baseResolver,
        QnResourcePool* resourcePool);

    ResourceAccessMap ensureAccessMap(const QnUuid& subjectId) const;
    bool isVideowallLayout(const QnLayoutResourcePtr& layout) const;
    bool isSharedLayout(const QnLayoutResourcePtr& layout) const;

private:
    void handleBaseAccessChanged(const QSet<QnUuid>& subjectIds);
    void handleReset();

    void handleLayoutAdded(const QnLayoutResourcePtr& layout);
    void handleLayoutRemoved(const QnLayoutResourcePtr& layout);
    void handleLayoutParentChanged(const QnResourcePtr& layoutResource, const QnUuid& oldParentId);
    void handleLayoutSharingMaybeChanged(const QnLayoutResourcePtr& layout);
    void handleLayoutShared(const QnLayoutResourcePtr& layout);
    void handleLayoutUnshared(const QnLayoutResourcePtr& layout);
    void handleCameraAdded(const QnVirtualCameraResourcePtr& camera);
    void handleCameraRemoved(const QnVirtualCameraResourcePtr& camera);

    void handleVideowallLayoutAddedOrRemoved(const QnVideoWallResourcePtr& videowall,
        const QnUuid& layoutId);

    void handleLayoutItemAddedOrRemoved(
        const QnUuid& resourceId, const QnLayoutResourcePtr& layout);

    void invalidateSubjects(const QSet<QnUuid>& subjectIds);

    QSet<QnUuid> invalidateCache(); //< Returns all subject ids that were cached.

    void notifyResolutionChanged(const QnLayoutResourcePtr& layout,
        const QSet<QnUuid>& knownAffectedSubjectIds,
        bool fromLayoutItem);

public:
    const QPointer<AbstractResourceAccessResolver> baseResolver;
    const QPointer<QnResourcePool> resourcePool;
    const std::shared_ptr<VideowallLayoutWatcher> videowallWatcher;
    mutable QHash<QnUuid, ResourceAccessMap> cachedAccessMaps;
    mutable QHash<QnLayoutResourcePtr, QSet<QnUuid>> layoutSubjects;
    mutable QHash<QnUuid, QSet<QnLayoutResourcePtr>> subjectLayouts;
    mutable nx::core::access::LayoutItemWatcher sharedLayoutItemsWatcher;
    QSet<QnUuid> desktopCameraIds;
    mutable nx::Mutex mutex;
};

// -----------------------------------------------------------------------------------------------
// LayoutItemAccessResolver

LayoutItemAccessResolver::LayoutItemAccessResolver(
    AbstractResourceAccessResolver* baseResolver,
    QnResourcePool* resourcePool,
    QObject* parent)
    :
    base_type(parent),
    d(new Private(this, baseResolver, resourcePool))
{
}

LayoutItemAccessResolver::~LayoutItemAccessResolver()
{
    // Required here for forward-declared scoped pointer destruction.
}

ResourceAccessMap LayoutItemAccessResolver::resourceAccessMap(const QnUuid& subjectId) const
{
    return d->baseResolver && d->resourcePool
        ? d->ensureAccessMap(subjectId)
        : ResourceAccessMap();
}

AccessRights LayoutItemAccessResolver::accessRights(
    const QnUuid& subjectId, const QnResourcePtr& resource) const
{
    if (!NX_ASSERT(resource) || !resource->resourcePool() || resource->hasFlags(Qn::removed))
        return {};

    if (!resource->hasFlags(Qn::desktop_camera))
        return base_type::accessRights(subjectId, resource);

    // Special processing for desktop cameras.

    NX_MUTEX_LOCKER lk(&d->mutex);
    const auto resourceLayouts = d->sharedLayoutItemsWatcher.resourceLayouts(resource->getId());
    lk.unlock();

    constexpr AccessRights kAllowedRights = AccessRight::viewLive | AccessRight::listenToAudio;

    AccessRights result;
    for (const auto& layout: nx::utils::keyRange(resourceLayouts))
    {
        if (d->videowallWatcher->layoutVideowall(layout))
            result |= accessRights(subjectId, layout);

        if ((result & kAllowedRights) == kAllowedRights)
            break;
    }

    return result & kAllowedRights;
}

GlobalPermissions LayoutItemAccessResolver::globalPermissions(const QnUuid& subjectId) const
{
    return d->baseResolver
        ? d->baseResolver->globalPermissions(subjectId)
        : GlobalPermissions{};
}

ResourceAccessDetails LayoutItemAccessResolver::accessDetails(
    const QnUuid& subjectId,
    const QnResourcePtr& resource,
    AccessRight accessRight) const
{
    if (!d->baseResolver || !NX_ASSERT(resource))
        return {};

    const bool hasAccess = accessRights(subjectId, resource).testFlag(accessRight);
    if (!hasAccess)
        return {};

    ResourceAccessDetails details = d->baseResolver->accessDetails(
        subjectId, resource, accessRight);

    NX_MUTEX_LOCKER lk(&d->mutex);
    const auto resourceLayouts = d->sharedLayoutItemsWatcher.resourceLayouts(resource->getId());
    lk.unlock();

    for (const auto& layout: nx::utils::keyRange(resourceLayouts))
    // After the `accessRights` call, `subjectLayouts` are guaranteed valid.
    {
        if (accessRights(subjectId, layout).testFlag(accessRight))
        {
            const QnResourcePtr videowall = d->videowallWatcher->layoutVideowall(layout);
            details[subjectId].insert(videowall ? videowall : (QnResourcePtr) layout);
        }
    }

    return details;
}

// -----------------------------------------------------------------------------------------------
// LayoutItemAccessResolver::Private

LayoutItemAccessResolver::Private::Private(
    LayoutItemAccessResolver* q,
    AbstractResourceAccessResolver* baseResolver,
    QnResourcePool* resourcePool)
    :
    q(q),
    baseResolver(baseResolver),
    resourcePool(resourcePool),
    videowallWatcher(VideowallLayoutWatcher::instance(resourcePool))
{
    NX_CRITICAL(baseResolver && resourcePool && videowallWatcher);

    connect(q->notifier(), &Notifier::subjectsSubscribed,
        baseResolver->notifier(), &Notifier::subscribeSubjects, Qt::DirectConnection);

    connect(q->notifier(), &Notifier::subjectsReleased,
        baseResolver->notifier(), &Notifier::releaseSubjects, Qt::DirectConnection);

    connect(baseResolver->notifier(), &Notifier::resourceAccessChanged,
        this, &Private::handleBaseAccessChanged, Qt::DirectConnection);

    connect(baseResolver->notifier(), &Notifier::resourceAccessReset,
        this, &Private::handleReset, Qt::DirectConnection);

    connect(resourcePool, &QnResourcePool::resourceAdded, this,
        [this](const QnResourcePtr& resource)
        {
            if (const auto layout = resource.objectCast<QnLayoutResource>())
                handleLayoutAdded(layout);
            else if (const auto camera = resource.objectCast<QnVirtualCameraResource>())
                handleCameraAdded(camera);
        }, Qt::DirectConnection);

    connect(resourcePool, &QnResourcePool::resourceRemoved, this,
        [this](const QnResourcePtr& resource)
        {
            if (const auto layout = resource.objectCast<QnLayoutResource>())
                handleLayoutRemoved(layout);
            else if (const auto camera = resource.objectCast<QnVirtualCameraResource>())
                handleCameraRemoved(camera);
        }, Qt::DirectConnection);

    connect(videowallWatcher.get(), &VideowallLayoutWatcher::videowallLayoutAdded,
        this, &Private::handleVideowallLayoutAddedOrRemoved, Qt::DirectConnection);

    connect(&sharedLayoutItemsWatcher, &LayoutItemWatcher::addedToLayout,
        this, &Private::handleLayoutItemAddedOrRemoved, Qt::DirectConnection);

    connect(&sharedLayoutItemsWatcher, &LayoutItemWatcher::removedFromLayout,
        this, &Private::handleLayoutItemAddedOrRemoved, Qt::DirectConnection);

    const auto layouts = resourcePool->getResources<QnLayoutResource>();
    for (const auto& layout: layouts)
        handleLayoutAdded(layout);
}

ResourceAccessMap LayoutItemAccessResolver::Private::ensureAccessMap(const QnUuid& subjectId) const
{
    NX_MUTEX_LOCKER lk(&mutex);

    if (cachedAccessMaps.contains(subjectId))
        return cachedAccessMaps.value(subjectId);

    const auto baseAccessMap = baseResolver->resourceAccessMap(subjectId);

    ResourceAccessMap& cachedAccessMapRef = cachedAccessMaps[subjectId];
    cachedAccessMapRef = baseAccessMap;

    // Revoke direct desktop cameras access.
    for (const auto desktopCameraId: desktopCameraIds)
        cachedAccessMapRef.remove(desktopCameraId);

    const auto layouts = resourcePool->getResourcesByIds<QnLayoutResource>(
        baseAccessMap.keys());

    ResourceAccessMap accessMap;
    for (const auto& layout: layouts)
    {
        const bool videowallLayout = isVideowallLayout(layout);
        if (!videowallLayout && !isSharedLayout(layout))
        {
            cachedAccessMapRef.remove(layout->getId());
            continue;
        }

        layoutSubjects[layout].insert(subjectId);
        subjectLayouts[subjectId].insert(layout);

        const auto accessRights = baseAccessMap.value(layout->getId());
        const auto layoutItems = layout->getItems();
        const bool allowDesktopCameras = videowallLayout;

        for (const auto& item: layoutItems)
        {
            if (!item.resource.id.isNull()
                && (allowDesktopCameras || !desktopCameraIds.contains(item.resource.id)))
            {
                accessMap[item.resource.id] = accessRights;
            }
        }
    }

    q->notifier()->subscribeSubjects({subjectId});
    cachedAccessMapRef += accessMap;

    NX_DEBUG(q, "Resolved and cached an access map for %1", subjectId);
    NX_VERBOSE(q, cachedAccessMapRef);

    return cachedAccessMapRef;
}

bool LayoutItemAccessResolver::Private::isVideowallLayout(const QnLayoutResourcePtr& layout) const
{
    return !videowallWatcher->layoutVideowall(layout).isNull();
}

bool LayoutItemAccessResolver::Private::isSharedLayout(const QnLayoutResourcePtr& layout) const
{
    return layout
        && layout->getParentId().isNull()
        && !videowallWatcher->isVideowallItem(layout->getId());
}

void LayoutItemAccessResolver::Private::handleBaseAccessChanged(const QSet<QnUuid>& subjectIds)
{
    NX_DEBUG(q, "Base resolution changed for %1 subjects: %2", subjectIds.size(), subjectIds);
    invalidateSubjects(subjectIds);
    q->notifyAccessChanged(subjectIds);
}

void LayoutItemAccessResolver::Private::invalidateSubjects(const QSet<QnUuid>& subjectIds)
{
    QSet<QnUuid> affectedCachedSubjectIds;
    {
        NX_MUTEX_LOCKER lk(&mutex);

        for (const auto& subjectId: subjectIds)
        {
            if (cachedAccessMaps.remove(subjectId))
                affectedCachedSubjectIds.insert(subjectId);

            for (const auto& layout: subjectLayouts.value(subjectId))
            {
                auto& layoutSubjectsRef = layoutSubjects[layout];
                layoutSubjectsRef.remove(subjectId);
                if (layoutSubjectsRef.empty())
                    layoutSubjects.remove(layout);
            }

            subjectLayouts.remove(subjectId);
        }
    }

    if (affectedCachedSubjectIds.empty())
        return;

    NX_DEBUG(q, "Cache invalidated for %1 subjects: %2",
        affectedCachedSubjectIds.size(), affectedCachedSubjectIds);

    baseResolver->notifier()->releaseSubjects(affectedCachedSubjectIds);
}

void LayoutItemAccessResolver::Private::notifyResolutionChanged(const QnLayoutResourcePtr& layout,
    const QSet<QnUuid>& knownAffectedSubjectIds, bool fromLayoutItem)
{
    if (!fromLayoutItem && isVideowallLayout(layout))
        return; //< Should be already notified by the base resolver.

    const auto watchedSubjectIds = q->notifier()->watchedSubjectIds();
    QSet<QnUuid> affectedWatchedSubjectIds;

    for (const auto id: watchedSubjectIds)
    {
        if (knownAffectedSubjectIds.contains(id) || baseResolver->accessRights(id, layout))
            affectedWatchedSubjectIds.insert(id);
    }

    q->notifyAccessChanged(affectedWatchedSubjectIds);
}

QSet<QnUuid> LayoutItemAccessResolver::Private::invalidateCache()
{
    NX_MUTEX_LOCKER lk(&mutex);

    const QSet<QnUuid> cachedSubjectIds(cachedAccessMaps.keyBegin(), cachedAccessMaps.keyEnd());

    cachedAccessMaps.clear();
    layoutSubjects.clear();
    subjectLayouts.clear();

    return cachedSubjectIds;
}

void LayoutItemAccessResolver::Private::handleReset()
{
    const auto affectedCachedSubjectIds = invalidateCache();
    NX_DEBUG(q, "Base resolution reset, whole cache invalidated");
    baseResolver->notifier()->releaseSubjects(affectedCachedSubjectIds);
    q->notifyAccessReset();
}

void LayoutItemAccessResolver::Private::handleLayoutAdded(const QnLayoutResourcePtr& layout)
{
    const bool isShared = isSharedLayout(layout);
    const bool isVideowall = !isShared && isVideowallLayout(layout);

    NX_VERBOSE(q, "Layout %1 added, shared=%2, videowall=%3", layout, isShared, isVideowall);

    connect(layout.get(), &QnResource::parentIdChanged,
        this, &Private::handleLayoutParentChanged, Qt::DirectConnection);

    if (isShared || isVideowall)
        handleLayoutShared(layout);
}

void LayoutItemAccessResolver::Private::handleLayoutRemoved(const QnLayoutResourcePtr& layout)
{
    const bool isShared = isSharedLayout(layout);
    const bool isVideowall = !isShared && isVideowallLayout(layout);

    NX_VERBOSE(q, "Layout %1 removed, shared=%2, videowall=%3", layout, isShared, isVideowall);

    layout->disconnect(this);

    if (isShared || isVideowall)
        handleLayoutUnshared(layout);
}

void LayoutItemAccessResolver::Private::handleLayoutParentChanged(
    const QnResourcePtr& layoutResource, const QnUuid& oldParentId)
{
    const auto layout = layoutResource.objectCast<QnLayoutResource>();
    if (!NX_ASSERT(layout))
        return;

    NX_VERBOSE(q, "Layout %1 parentId changed from %2 to %3",
        layout, oldParentId, layout->getParentId());

    handleLayoutSharingMaybeChanged(layout);
}

void LayoutItemAccessResolver::Private::handleVideowallLayoutAddedOrRemoved(
    const QnVideoWallResourcePtr& videowall, const QnUuid& layoutId)
{
    const auto layout = resourcePool
        ? resourcePool->getResourceById<QnLayoutResource>(layoutId)
        : QnLayoutResourcePtr();

    NX_VERBOSE(q, "Videowall %1 layout %2 added or removed, %3", videowall, layoutId,
        layout
            ? QString("in the pool, parentId=%1").arg(layout->getParentId().toString())
            : QString("not in the pool"));

    if (layout)
        handleLayoutSharingMaybeChanged(layout);
}

void LayoutItemAccessResolver::Private::handleLayoutSharingMaybeChanged(
    const QnLayoutResourcePtr& layout)
{
    if (isSharedLayout(layout) || isVideowallLayout(layout))
        handleLayoutShared(layout);
    else if (sharedLayoutItemsWatcher.isWatched(layout))
        handleLayoutUnshared(layout);
}

void LayoutItemAccessResolver::Private::handleLayoutShared(const QnLayoutResourcePtr& layout)
{
    if (!baseResolver)
        return;

    const QSignalBlocker signalBlocker(&sharedLayoutItemsWatcher);
    sharedLayoutItemsWatcher.addWatchedLayout(layout);

    QSet<QnUuid> affectedCachedSubjectIds;
    {
        NX_MUTEX_LOCKER lk(&mutex);

        for (const auto& subjectId: nx::utils::keyRange(cachedAccessMaps))
        {
            if (baseResolver->accessRights(subjectId, layout))
                affectedCachedSubjectIds.insert(subjectId);
        }

        for (const auto subjectId: affectedCachedSubjectIds)
            cachedAccessMaps.remove(subjectId);
    }

    NX_DEBUG(q, "Layout %1 became shared (either directly or via videowall), "
        "cache invalidated for %2 affected subjects: %3",
        layout, affectedCachedSubjectIds.size(), affectedCachedSubjectIds);

    baseResolver->notifier()->releaseSubjects(affectedCachedSubjectIds);

    notifyResolutionChanged(layout,
        /*knownAffectedSubjectIds*/ affectedCachedSubjectIds,
        /*fromLayoutItem*/ false);
}

void LayoutItemAccessResolver::Private::handleLayoutUnshared(const QnLayoutResourcePtr& layout)
{
    const QSignalBlocker signalBlocker(&sharedLayoutItemsWatcher);
    sharedLayoutItemsWatcher.removeWatchedLayout(layout);

    const auto invalidateLayoutData =
        [this](const QnLayoutResourcePtr& layout) -> QSet<QnUuid> /*layoutSubjectIds*/
        {
            NX_MUTEX_LOCKER lk(&mutex);
            const auto subjectIds = layoutSubjects.take(layout);

            for (const auto& subjectId: subjectIds)
            {
                subjectLayouts[subjectId].remove(layout);
                cachedAccessMaps.remove(subjectId);
            }

            return subjectIds;
        };

    const auto affectedCachedSubjectIds = invalidateLayoutData(layout);

    NX_DEBUG(q, "Layout %1 stopped being shared (either directly or via videowall), "
        "cache invalidated for %2 affected subjects: %3",
        layout, affectedCachedSubjectIds.size(), affectedCachedSubjectIds);

    baseResolver->notifier()->releaseSubjects(affectedCachedSubjectIds);

    notifyResolutionChanged(layout,
        /*knownAffectedSubjectIds*/ affectedCachedSubjectIds,
        /*fromLayoutItem*/ false);
}

void LayoutItemAccessResolver::Private::handleCameraAdded(const QnVirtualCameraResourcePtr& camera)
{
    if (!NX_ASSERT(camera) || !camera->hasFlags(Qn::desktop_camera))
        return;

    const auto cameraId = camera->getId();
    NX_DEBUG(q, "Added desktop camera %1", camera);

    QSet<QnUuid> affectedSubjectIds;
    {
        NX_MUTEX_LOCKER lk(&mutex);
        desktopCameraIds.insert(cameraId);

        const auto layouts = sharedLayoutItemsWatcher.resourceLayouts(cameraId);
        for (const auto layout: nx::utils::keyRange(layouts))
        {
            if (!isVideowallLayout(layout))
                affectedSubjectIds += layoutSubjects.value(layout);
        }
    }

    invalidateSubjects(affectedSubjectIds);
    q->notifyAccessChanged(affectedSubjectIds);
}

void LayoutItemAccessResolver::Private::handleCameraRemoved(const QnVirtualCameraResourcePtr& camera)
{
    if (!NX_ASSERT(camera) || !camera->hasFlags(Qn::desktop_camera))
        return;

    NX_DEBUG(q, "Removed desktop camera %1", camera);

    NX_MUTEX_LOCKER lk(&mutex);
    desktopCameraIds.remove(camera->getId());
}

void LayoutItemAccessResolver::Private::handleLayoutItemAddedOrRemoved(
    const QnUuid& /*resourceId*/, const QnLayoutResourcePtr& layout)
{
    if (!NX_ASSERT(isSharedLayout(layout) || isVideowallLayout(layout)))
        return;

    const auto invalidateLayoutSubjects =
        [this](const QnLayoutResourcePtr& layout) -> QSet<QnUuid> /*layoutSubjectIds*/
        {
            NX_MUTEX_LOCKER lk(&mutex);
            const auto subjectIds = layoutSubjects.take(layout);

            for (const auto& subjectId: subjectIds)
                cachedAccessMaps.remove(subjectId);

            return subjectIds;
        };


    const auto affectedCachedSubjectIds = invalidateLayoutSubjects(layout);

    NX_DEBUG(q, "Layout %1 item resources changed, cache invalidated for %2 affected subjects: %3",
        layout, affectedCachedSubjectIds.size(), affectedCachedSubjectIds);

    baseResolver->notifier()->releaseSubjects(affectedCachedSubjectIds);

    notifyResolutionChanged(layout,
        /*knownAffectedSubjectIds*/ affectedCachedSubjectIds,
        /*fromLayoutItem*/ true);
}

} // namespace nx::core::access
