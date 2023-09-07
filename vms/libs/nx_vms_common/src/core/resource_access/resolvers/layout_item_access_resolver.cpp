// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_item_access_resolver.h"

#include <QtCore/QPointer>

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
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
    explicit Private(
        LayoutItemAccessResolver* q,
        AbstractResourceAccessResolver* baseResolver,
        QnResourcePool* resourcePool,
        bool subjectEditingMode);

    ResourceAccessMap ensureAccessMap(const QnUuid& subjectId) const;
    bool isRelevantLayout(const QnLayoutResourcePtr& layout) const;
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
        const QnUuid& resourceId, const QnLayoutResourcePtr& storedLayout);

    void invalidateSubjects(const QSet<QnUuid>& subjectIds);

    QSet<QnUuid> invalidateCache(); //< Returns all subject ids that were cached.

    void notifyResolutionChanged(const QnLayoutResourcePtr& layout,
        const QSet<QnUuid>& knownAffectedSubjectIds,
        bool fromLayoutItem);

    // For logging.
    QString layoutType(const QnLayoutResourcePtr& layout) const;
    QString layoutInfo(const QnLayoutResourcePtr& layout) const;

public:
    const QPointer<AbstractResourceAccessResolver> baseResolver;
    const QPointer<QnResourcePool> resourcePool;
    const bool subjectEditingMode;
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
    bool subjectEditingMode,
    QObject* parent)
    :
    base_type(parent),
    d(new Private(this, baseResolver, resourcePool, subjectEditingMode))
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

    constexpr AccessRights kAllowedRights = AccessRight::view;

    AccessRights result;
    for (const auto& storedLayout: nx::utils::keyRange(resourceLayouts))
    {
        const auto layout = storedLayout->transientLayout();

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

    if (hasFullAccessRights(subjectId) && !resource->hasFlags(Qn::desktop_camera))
        return details;

    NX_MUTEX_LOCKER lk(&d->mutex);
    const auto resourceLayouts = d->sharedLayoutItemsWatcher.resourceLayouts(resource->getId());
    lk.unlock();

    for (const auto& storedLayout: nx::utils::keyRange(resourceLayouts))
    {
        const auto layout = storedLayout->transientLayout();

        if (accessRights(subjectId, layout).testFlag(accessRight))
        {
            const auto parentResource = layout->getParentResource();
            const bool isLocalLayout = parentResource.objectCast<QnUserResource>() != nullptr;
            NX_ASSERT(d->subjectEditingMode || !isLocalLayout);

            details[subjectId].insert(parentResource && !isLocalLayout
                ? parentResource
                : (QnResourcePtr) layout);
        }
    }

    return details;
}

// -----------------------------------------------------------------------------------------------
// LayoutItemAccessResolver::Private

LayoutItemAccessResolver::Private::Private(
    LayoutItemAccessResolver* q,
    AbstractResourceAccessResolver* baseResolver,
    QnResourcePool* resourcePool,
    bool subjectEditingMode)
    :
    q(q),
    baseResolver(baseResolver),
    resourcePool(resourcePool),
    subjectEditingMode(subjectEditingMode),
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

    connect(resourcePool, &QnResourcePool::resourcesAdded, this,
        [this](const QnResourceList& resources)
        {
            for (const auto& resource: resources)
            {
                if (const auto layout = resource.objectCast<QnLayoutResource>())
                    handleLayoutAdded(layout);
                else if (const auto camera = resource.objectCast<QnVirtualCameraResource>())
                    handleCameraAdded(camera);
            }
        }, Qt::DirectConnection);

    connect(resourcePool, &QnResourcePool::resourcesRemoved, this,
        [this](const QnResourceList& resources)
        {
            for (const auto& resource: resources)
            {
                if (const auto layout = resource.objectCast<QnLayoutResource>())
                    handleLayoutRemoved(layout);
                else if (const auto camera = resource.objectCast<QnVirtualCameraResource>())
                    handleCameraRemoved(camera);
            }
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
        if (!isRelevantLayout(layout))
        {
            cachedAccessMapRef.remove(layout->getId());
            continue;
        }

        layoutSubjects[layout].insert(subjectId);
        subjectLayouts[subjectId].insert(layout);

        const auto accessRights = baseAccessMap.value(layout->getId());
        const auto layoutItems = layout->storedLayout()->getItems();
        const bool allowDesktopCameras = isVideowallLayout(layout);

        for (const auto& item: layoutItems)
        {
            if (!item.resource.id.isNull()
                && (allowDesktopCameras || !desktopCameraIds.contains(item.resource.id)))
            {
                accessMap[item.resource.id] |= accessRights;
            }
        }
    }

    q->notifier()->subscribeSubjects({subjectId});
    cachedAccessMapRef += accessMap;

    NX_DEBUG(q, "Resolved and cached an access map for %1", subjectId);
    NX_VERBOSE(q, toString(cachedAccessMapRef, resourcePool));

    return cachedAccessMapRef;
}

bool LayoutItemAccessResolver::Private::isRelevantLayout(const QnLayoutResourcePtr& layout) const
{
    return isSharedLayout(layout) || isVideowallLayout(layout);
}

bool LayoutItemAccessResolver::Private::isVideowallLayout(const QnLayoutResourcePtr& layout) const
{
    return !videowallWatcher->layoutVideowall(layout).isNull();
}

bool LayoutItemAccessResolver::Private::isSharedLayout(const QnLayoutResourcePtr& layout) const
{
    if (layout->isFile())
        return false;

    if (subjectEditingMode)
        return !isVideowallLayout(layout);

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
    {
        NX_VERBOSE(q, "Skipping notification: already notified by the base resolver");
        return;
    }

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

QString LayoutItemAccessResolver::Private::layoutType(const QnLayoutResourcePtr& layout) const
{
    if (!NX_ASSERT(layout))
        return {};

    if (isSharedLayout(layout))
        return "shared";

    if (isVideowallLayout(layout))
        return "videowall";

    if (!layout->getParentResource())
        return "unresolved";

    return "other";
}

QString LayoutItemAccessResolver::Private::layoutInfo(const QnLayoutResourcePtr& layout) const
{
    return NX_FMT("Layout %1, parent %2, type=%3",
        layout,
        toLogString(layout->getParentId(), resourcePool),
        layoutType(layout));
}

void LayoutItemAccessResolver::Private::handleLayoutAdded(const QnLayoutResourcePtr& layout)
{
    NX_VERBOSE(q, "%1 added to the pool", layoutInfo(layout));

    connect(layout.get(), &QnResource::parentIdChanged,
        this, &Private::handleLayoutParentChanged, Qt::DirectConnection);

    if (isRelevantLayout(layout))
        handleLayoutShared(layout);
}

void LayoutItemAccessResolver::Private::handleLayoutRemoved(const QnLayoutResourcePtr& layout)
{
    NX_VERBOSE(q, "%1 removed from the pool", layoutInfo(layout));

    layout->disconnect(this);

    if (isRelevantLayout(layout))
        handleLayoutUnshared(layout);
}

void LayoutItemAccessResolver::Private::handleLayoutParentChanged(
    const QnResourcePtr& layoutResource, const QnUuid& oldParentId)
{
    const auto layout = layoutResource.objectCast<QnLayoutResource>();
    if (!NX_ASSERT(layout))
        return;

    NX_VERBOSE(q, "Layout %1 parent changed from %2 to %3",
        layout,
        toLogString(oldParentId, resourcePool),
        toLogString(layout->getParentId(), resourcePool));

    handleLayoutSharingMaybeChanged(layout);
}

void LayoutItemAccessResolver::Private::handleVideowallLayoutAddedOrRemoved(
    const QnVideoWallResourcePtr& videowall, const QnUuid& layoutId)
{
    const auto layout = resourcePool
        ? resourcePool->getResourceById<QnLayoutResource>(layoutId)
        : QnLayoutResourcePtr();

    if (layout)
    {
        NX_VERBOSE(q, "Videowall %1 %2 added or removed", videowall, layoutInfo(layout));
        handleLayoutSharingMaybeChanged(layout);
    }
    else
    {
        NX_VERBOSE(q, "Videowall %1 layout %2 (not in the pool) added or removed",
            videowall, layoutId);
    }
}

void LayoutItemAccessResolver::Private::handleLayoutSharingMaybeChanged(
    const QnLayoutResourcePtr& layout)
{
    if (isRelevantLayout(layout))
        handleLayoutShared(layout);
    else if (sharedLayoutItemsWatcher.isWatched(layout->storedLayout()))
        handleLayoutUnshared(layout);
}

void LayoutItemAccessResolver::Private::handleLayoutShared(const QnLayoutResourcePtr& layout)
{
    if (!baseResolver)
        return;

    const QSignalBlocker signalBlocker(&sharedLayoutItemsWatcher);
    sharedLayoutItemsWatcher.addWatchedLayout(layout->storedLayout());

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

    NX_DEBUG(q, "%1 became relevant, %2",
        layoutInfo(layout), affectedCacheToLogString(affectedCachedSubjectIds));

    baseResolver->notifier()->releaseSubjects(affectedCachedSubjectIds);

    notifyResolutionChanged(layout,
        /*knownAffectedSubjectIds*/ affectedCachedSubjectIds,
        /*fromLayoutItem*/ false);
}

void LayoutItemAccessResolver::Private::handleLayoutUnshared(const QnLayoutResourcePtr& layout)
{
    const QSignalBlocker signalBlocker(&sharedLayoutItemsWatcher);
    sharedLayoutItemsWatcher.removeWatchedLayout(layout->storedLayout());

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

    NX_DEBUG(q, "%1 stopped being relevant, %2",
        layoutInfo(layout), affectedCacheToLogString(affectedCachedSubjectIds));

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

        const auto resourceLayouts = sharedLayoutItemsWatcher.resourceLayouts(cameraId);
        for (const auto storedLayout: nx::utils::keyRange(resourceLayouts))
        {
            const auto layout = storedLayout->transientLayout();

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
    const QnUuid& /*resourceId*/, const QnLayoutResourcePtr& storedLayout)
{
    const auto layout = storedLayout->transientLayout();

    if (!NX_ASSERT(isRelevantLayout(layout)))
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

    NX_DEBUG(q, "Layout %1 items changed, %2",
        layout, affectedCacheToLogString(affectedCachedSubjectIds));

    baseResolver->notifier()->releaseSubjects(affectedCachedSubjectIds);

    notifyResolutionChanged(layout,
        /*knownAffectedSubjectIds*/ affectedCachedSubjectIds,
        /*fromLayoutItem*/ true);
}

} // namespace nx::core::access
