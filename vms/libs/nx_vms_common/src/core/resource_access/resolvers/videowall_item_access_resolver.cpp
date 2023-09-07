// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "videowall_item_access_resolver.h"

#include <QtCore/QPointer>

#include <core/resource/layout_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/utils/range_adapters.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/common/utils/videowall_layout_watcher.h>
#include <utils/common/counter_hash.h>

namespace nx::core::access {

namespace {

constexpr AccessRight kRequiredAccessRight = AccessRight::edit;
constexpr AccessRight kGrantedAccessRight = AccessRight::view;

} // namespace

using namespace nx::vms::api;
using namespace nx::vms::common;

class VideowallItemAccessResolver::Private: public QObject
{
    VideowallItemAccessResolver* const q;

public:
    explicit Private(VideowallItemAccessResolver* q,
        AbstractResourceAccessResolver* baseResolver,
        QnResourcePool* resourcePool);

    ResourceAccessMap ensureAccessMap(const QnUuid& subjectId) const;

    void handleBaseAccessChanged(const QSet<QnUuid>& subjectIds);
    void handleReset();

    void handleVideowallAdded(const QnVideoWallResourcePtr& videowall);
    void handleVideowallRemoved(const QnVideoWallResourcePtr& videowall);
    void handleVideowallLayoutsChanged(const QnVideoWallResourcePtr& videowall);

    void handleVideowallLayoutAdded(const QnVideoWallResourcePtr& videowall,
        const QnUuid& layoutId);

    void handleVideowallLayoutRemoved(const QnVideoWallResourcePtr& videowall,
        const QnUuid& layoutId);

    void handleLayoutAdded(const QnLayoutResourcePtr& layout);
    void handleLayoutRemoved(const QnLayoutResourcePtr& layout);
    void handleLayoutParentChanged(const QnResourcePtr& layoutResource, const QnUuid& oldParentId);

    void invalidateSubjects(const QSet<QnUuid>& subjectIds);

    QSet<QnUuid> invalidateCache(); //< Returns all subject ids that were cached.

    void notifyResolutionChanged(const QnVideoWallResourcePtr& videowall,
        const QSet<QnUuid>& knownAffectedSubjectIds);

public:
    const QPointer<AbstractResourceAccessResolver> baseResolver;
    const QPointer<QnResourcePool> resourcePool;
    const std::shared_ptr<VideowallLayoutWatcher> videowallWatcher;
    mutable QHash<QnUuid, ResourceAccessMap> cachedAccessMaps;
    mutable QHash<QnVideoWallResourcePtr, QSet<QnUuid>> videowallSubjects;
    mutable QHash<QnUuid, QSet<QnVideoWallResourcePtr>> subjectVideowalls;
    mutable nx::Mutex mutex;
};

// -----------------------------------------------------------------------------------------------
// VideowallItemAccessResolver

VideowallItemAccessResolver::VideowallItemAccessResolver(
    AbstractResourceAccessResolver* baseResolver,
    QnResourcePool* resourcePool,
    QObject* parent)
    :
    base_type(parent),
    d(new Private(this, baseResolver, resourcePool))
{
}

VideowallItemAccessResolver::~VideowallItemAccessResolver()
{
    // Required here for forward-declared scoped pointer destruction.
}

ResourceAccessMap VideowallItemAccessResolver::resourceAccessMap(const QnUuid& subjectId) const
{
    return d->baseResolver && d->resourcePool
        ? d->ensureAccessMap(subjectId)
        : ResourceAccessMap();
}

GlobalPermissions VideowallItemAccessResolver::globalPermissions(const QnUuid& subjectId) const
{
    return d->baseResolver
        ? d->baseResolver->globalPermissions(subjectId)
        : GlobalPermissions{};
}

ResourceAccessDetails VideowallItemAccessResolver::accessDetails(
    const QnUuid& subjectId,
    const QnResourcePtr& resource,
    nx::vms::api::AccessRight accessRight) const
{
    if (!d->baseResolver || !d->resourcePool || !NX_ASSERT(resource))
        return {};

    const bool hasAccess = accessRights(subjectId, resource).testFlag(accessRight);
    if (!hasAccess)
        return {};

    const auto layout = resource.objectCast<QnLayoutResource>();
    if (layout && accessRight == kGrantedAccessRight)
    {
        if (const auto videowall = d->videowallWatcher->layoutVideowall(layout))
        {
            NX_MUTEX_LOCKER lk(&d->mutex);
            if (d->subjectVideowalls.value(subjectId).contains(videowall))
                return d->baseResolver->accessDetails(subjectId, videowall, kRequiredAccessRight);
        }
    }

    return d->baseResolver->accessDetails(subjectId, resource, accessRight);
}

// -----------------------------------------------------------------------------------------------
// VideowallItemAccessResolver::Private

VideowallItemAccessResolver::Private::Private(
    VideowallItemAccessResolver* q,
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

    connect(videowallWatcher.get(), &VideowallLayoutWatcher::videowallAdded,
        this, &Private::handleVideowallAdded, Qt::DirectConnection);

    connect(videowallWatcher.get(), &VideowallLayoutWatcher::videowallRemoved,
        this, &Private::handleVideowallRemoved, Qt::DirectConnection);

    connect(videowallWatcher.get(), &VideowallLayoutWatcher::videowallLayoutAdded,
        this, &Private::handleVideowallLayoutAdded, Qt::DirectConnection);

    connect(videowallWatcher.get(), &VideowallLayoutWatcher::videowallLayoutRemoved,
        this, &Private::handleVideowallLayoutRemoved, Qt::DirectConnection);

    connect(resourcePool, &QnResourcePool::resourcesAdded, this,
        [this](const QnResourceList& resources)
        {
            for (const auto& resource: resources)
            {
                if (const auto layout = resource.objectCast<QnLayoutResource>())
                    handleLayoutAdded(layout);
            }
        }, Qt::DirectConnection);

    connect(resourcePool, &QnResourcePool::resourcesRemoved, this,
        [this](const QnResourceList& resources)
        {
            for (const auto& resource: resources)
            {
                if (const auto layout = resource.objectCast<QnLayoutResource>())
                    handleLayoutRemoved(layout);
            }
        }, Qt::DirectConnection);

    const auto layouts = resourcePool->getResources<QnLayoutResource>();
    for (const auto& layout: layouts)
    {
        connect(layout.get(), &QnResource::parentIdChanged,
            this, &Private::handleLayoutParentChanged, Qt::DirectConnection);
    }
}

ResourceAccessMap VideowallItemAccessResolver::Private::ensureAccessMap(
    const QnUuid& subjectId) const
{
    NX_MUTEX_LOCKER lk(&mutex);

    if (cachedAccessMaps.contains(subjectId))
        return cachedAccessMaps.value(subjectId);

    ResourceAccessMap& cachedAccessMapRef = cachedAccessMaps[subjectId];
    cachedAccessMapRef = baseResolver->resourceAccessMap(subjectId);

    ResourceAccessMap accessMap;

    const auto videowalls = resourcePool->getResources<QnVideoWallResource>();
    for (const auto& videowall: videowalls)
    {
        // Videowall layouts may not have own access rights.
        const auto videowallLayoutIds = videowallWatcher->videowallLayouts(videowall);
        for (const auto& layoutId: nx::utils::keyRange(videowallLayoutIds))
            cachedAccessMapRef.remove(layoutId);

        const auto accessRights = baseResolver->accessRights(subjectId, videowall);
        if (!accessRights.testFlag(kRequiredAccessRight))
            continue;

        videowallSubjects[videowall].insert(subjectId);
        subjectVideowalls[subjectId].insert(videowall);

        const auto layouts = resourcePool->getResourcesByIds<QnLayoutResource>(
            nx::utils::keyRange(videowallLayoutIds));

        const auto videowallId = videowall->getId();
        for (const auto& layout: layouts)
        {
            if (layout->getParentId() == videowallId)
                accessMap[layout->getId()] = kGrantedAccessRight;
        }
    }

    baseResolver->notifier()->subscribeSubjects({subjectId});
    cachedAccessMapRef += accessMap;

    NX_DEBUG(q, "Resolved and cached an access map for %1", subjectId);
    NX_VERBOSE(q, toString(cachedAccessMapRef, resourcePool));

    return cachedAccessMapRef;
}

void VideowallItemAccessResolver::Private::handleBaseAccessChanged(const QSet<QnUuid>& subjectIds)
{
    NX_DEBUG(q, "Base resolution changed for %1 subjects: %2", subjectIds.size(), subjectIds);
    invalidateSubjects(subjectIds);
    q->notifyAccessChanged(subjectIds);
}

void VideowallItemAccessResolver::Private::invalidateSubjects(const QSet<QnUuid>& subjectIds)
{
    QSet<QnUuid> affectedCachedSubjectIds;
    {
        NX_MUTEX_LOCKER lk(&mutex);

        for (const auto& subjectId: subjectIds)
        {
            if (cachedAccessMaps.remove(subjectId))
                affectedCachedSubjectIds.insert(subjectId);

            for (const auto& videowall: subjectVideowalls.value(subjectId))
            {
                auto& videowallSubjectsRef = videowallSubjects[videowall];
                videowallSubjectsRef.remove(subjectId);

                if (videowallSubjectsRef.empty())
                    videowallSubjects.remove(videowall);
            }

            subjectVideowalls.remove(subjectId);
        }
    }

    if (affectedCachedSubjectIds.empty())
        return;

    NX_DEBUG(q, "Cache invalidated for %1 subjects: %2",
        affectedCachedSubjectIds.size(), affectedCachedSubjectIds);

    baseResolver->notifier()->releaseSubjects(affectedCachedSubjectIds);
}

void VideowallItemAccessResolver::Private::notifyResolutionChanged(
    const QnVideoWallResourcePtr& videowall, const QSet<QnUuid>& knownAffectedSubjectIds)
{
    const auto watchedSubjectIds = q->notifier()->watchedSubjectIds();
    QSet<QnUuid> affectedWatchedSubjectIds;

    for (const auto id: watchedSubjectIds)
    {
        if (knownAffectedSubjectIds.contains(id) || baseResolver->accessRights(id, videowall))
            affectedWatchedSubjectIds.insert(id);
    }

    q->notifyAccessChanged(affectedWatchedSubjectIds);
}

void VideowallItemAccessResolver::Private::handleReset()
{
    const auto affectedCachedSubjectIds = invalidateCache();
    NX_DEBUG(q, "Base resolution reset, whole cache invalidated");
    baseResolver->notifier()->releaseSubjects(affectedCachedSubjectIds);
    q->notifyAccessReset();
}

QSet<QnUuid> VideowallItemAccessResolver::Private::invalidateCache()
{
    NX_MUTEX_LOCKER lk(&mutex);
    const auto cachedSubjectIds = QSet<QnUuid>(
        cachedAccessMaps.keyBegin(), cachedAccessMaps.keyEnd());

    cachedAccessMaps.clear();
    videowallSubjects.clear();
    subjectVideowalls.clear();

    return cachedSubjectIds;
}

void VideowallItemAccessResolver::Private::handleVideowallAdded(
    const QnVideoWallResourcePtr& videowall)
{
    if (!baseResolver)
        return;

    QSet<QnUuid> affectedCachedSubjectIds;
    {
        NX_MUTEX_LOCKER lk(&mutex);

        for (const auto& subjectId: nx::utils::keyRange(cachedAccessMaps))
        {
            if (baseResolver->accessRights(subjectId, videowall))
                affectedCachedSubjectIds.insert(subjectId);
        }

        for (const auto& subjectId: affectedCachedSubjectIds)
            cachedAccessMaps.remove(subjectId);
    }

    NX_DEBUG(q, "Videowall %1 added to the pool, %2",
        videowall, affectedCacheToLogString(affectedCachedSubjectIds));

    baseResolver->notifier()->releaseSubjects(affectedCachedSubjectIds);
    notifyResolutionChanged(videowall, /*knownAffectedSubjectIds*/ affectedCachedSubjectIds);
}

void VideowallItemAccessResolver::Private::handleVideowallRemoved(
    const QnVideoWallResourcePtr& videowall)
{
    const auto invalidateVideowallData =
        [this](const QnVideoWallResourcePtr& videowall) -> QSet<QnUuid> /*videowallSubjectIds*/
        {
            NX_MUTEX_LOCKER lk(&mutex);
            const auto subjectIds = videowallSubjects.take(videowall);

            for (const auto subjectId: subjectIds)
            {
                subjectVideowalls[subjectId].remove(videowall);
                cachedAccessMaps.remove(subjectId);
            }

            return subjectIds;
        };

    videowall->disconnect(this);
    const auto affectedCachedSubjectIds = invalidateVideowallData(videowall);

    NX_DEBUG(q, "Videowall %1 removed from the pool, %2",
        videowall, affectedCacheToLogString(affectedCachedSubjectIds));

    baseResolver->notifier()->releaseSubjects(affectedCachedSubjectIds);
    notifyResolutionChanged(videowall, /*knownAffectedSubjectIds*/ affectedCachedSubjectIds);
}

void VideowallItemAccessResolver::Private::handleVideowallLayoutAdded(
    const QnVideoWallResourcePtr& videowall, const QnUuid& layoutId)
{
    const auto layout = resourcePool
        ? resourcePool->getResourceById<QnLayoutResource>(layoutId)
        : QnLayoutResourcePtr();

    if (layout)
    {
        NX_VERBOSE(q, "Videowall %1 layout %2 added", videowall, layout);

        // Handle videowall change only if the layout is in the pool
        // and is already owned by the videowall.
        if (layout->getParentId() == videowall->getId())
            handleVideowallLayoutsChanged(videowall);
        else
            NX_VERBOSE(q, "The layout isn't owned by the videowall yet");
    }
    else
    {
        NX_VERBOSE(q, "Videowall %1 layout %2 (not in the pool) added",
            videowall, layoutId);
    }
}

void VideowallItemAccessResolver::Private::handleVideowallLayoutRemoved(
    const QnVideoWallResourcePtr& videowall, const QnUuid& layoutId)
{
    const auto layout = resourcePool
        ? resourcePool->getResourceById<QnLayoutResource>(layoutId)
        : QnLayoutResourcePtr();

    if (layout)
    {
        NX_VERBOSE(q, "Videowall %1 layout %2 removed", videowall, layout);
    }
    else
    {
        NX_VERBOSE(q, "Videowall %1 layout %2 (not in the pool) removed",
            videowall, layoutId);
    }

    // Handle videowall change in two cases:
    // - a videowall layout was removed from its items
    // - a videowall layout was removed from the resource pool
    handleVideowallLayoutsChanged(videowall);
}

void VideowallItemAccessResolver::Private::handleVideowallLayoutsChanged(
    const QnVideoWallResourcePtr& videowall)
{
    const auto invalidateVideowallSubjects =
        [this](const QnVideoWallResourcePtr& videowall) -> QSet<QnUuid> /*videowallSubjectIds*/
        {
            NX_MUTEX_LOCKER lk(&mutex);
            const auto subjectIds = videowallSubjects.take(videowall);

            for (const auto subjectId: subjectIds)
                cachedAccessMaps.remove(subjectId);

            return subjectIds;
        };

    const auto affectedCachedSubjectIds = invalidateVideowallSubjects(videowall);

    NX_DEBUG(q, "Videowall %1 set of layouts changed, %2",
        videowall, affectedCacheToLogString(affectedCachedSubjectIds));

    baseResolver->notifier()->releaseSubjects(affectedCachedSubjectIds);
    notifyResolutionChanged(videowall, /*knownAffectedSubjectIds*/ affectedCachedSubjectIds);
}

void VideowallItemAccessResolver::Private::handleLayoutAdded(const QnLayoutResourcePtr& layout)
{
    NX_VERBOSE(q, "Layout %1 added to the pool", layout);

    connect(layout.get(), &QnResource::parentIdChanged,
        this, &Private::handleLayoutParentChanged, Qt::DirectConnection);

    if (const auto videowall = videowallWatcher->layoutVideowall(layout))
        handleVideowallLayoutsChanged(videowall);
}

void VideowallItemAccessResolver::Private::handleLayoutRemoved(const QnLayoutResourcePtr& layout)
{
    NX_VERBOSE(q, "Layout %1 removed from the pool", layout);

    layout->disconnect(this);

    if (const auto videowall = videowallWatcher->layoutVideowall(layout))
        handleVideowallLayoutsChanged(videowall);
}

void VideowallItemAccessResolver::Private::handleLayoutParentChanged(
    const QnResourcePtr& layoutResource, const QnUuid& oldParentId)
{
    const auto layout = layoutResource.objectCast<QnLayoutResource>();
    if (!NX_ASSERT(layout) || !resourcePool)
        return;

    NX_VERBOSE(q, "Layout %1 parent changed from %2 to %3",
        layout,
        toLogString(oldParentId, resourcePool),
        toLogString(layout->getParentId(), resourcePool));

    if (const auto oldVideowall = resourcePool->getResourceById<QnVideoWallResource>(oldParentId))
    {
        if (videowallWatcher->videowallLayouts(oldVideowall).contains(layout->getId()))
            handleVideowallLayoutsChanged(oldVideowall);
    }

    if (const auto newVideowall = videowallWatcher->layoutVideowall(layout))
        handleVideowallLayoutsChanged(newVideowall);
}

} // namespace nx::core::access
