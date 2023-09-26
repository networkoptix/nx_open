// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "intercom_layout_access_resolver.h"

#include <QtCore/QPointer>

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/utils/range_adapters.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/common/intercom/utils.h>

namespace nx::core::access {

using namespace nx::vms::api;
using namespace nx::vms::common;

class IntercomLayoutAccessResolver::Private: public QObject
{
    IntercomLayoutAccessResolver* const q;

public:
    explicit Private(IntercomLayoutAccessResolver* q,
        AbstractResourceAccessResolver* baseResolver,
        QnResourcePool* resourcePool);

    ResourceAccessMap ensureAccessMap(const QnUuid& subjectId) const;

    void handleBaseAccessChanged(const QSet<QnUuid>& subjectIds);
    void handleReset();

    void handleResourcesAdded(const QnResourceList& resources);
    void handleResourcesRemoved(const QnResourceList& resources);
    void handleIntercomsAddedOrRemoved(const QnVirtualCameraResourceList& intercoms, bool isAdded);

    void invalidateSubjects(const QSet<QnUuid>& subjectIds);

    QSet<QnUuid> invalidateCache(); //< Returns all subject ids that were cached.

    void notifyResolutionChanged(const QnVirtualCameraResourceList& intercoms,
        const QSet<QnUuid>& knownAffectedSubjectIds);

public:
    const QPointer<AbstractResourceAccessResolver> baseResolver;
    const QPointer<QnResourcePool> resourcePool;
    QSet<QnVirtualCameraResourcePtr> allIntercoms;
    mutable QHash<QnUuid, ResourceAccessMap> cachedAccessMaps;
    mutable nx::Mutex mutex;
};

// -----------------------------------------------------------------------------------------------
// IntercomLayoutAccessResolver

IntercomLayoutAccessResolver::IntercomLayoutAccessResolver(
    AbstractResourceAccessResolver* baseResolver,
    QnResourcePool* resourcePool,
    QObject* parent)
    :
    base_type(parent),
    d(new Private(this, baseResolver, resourcePool))
{
}

IntercomLayoutAccessResolver::~IntercomLayoutAccessResolver()
{
    // Required here for forward-declared scoped pointer destruction.
}

ResourceAccessMap IntercomLayoutAccessResolver::resourceAccessMap(const QnUuid& subjectId) const
{
    return d->baseResolver && d->resourcePool
        ? d->ensureAccessMap(subjectId)
        : ResourceAccessMap();
}

GlobalPermissions IntercomLayoutAccessResolver::globalPermissions(const QnUuid& subjectId) const
{
    return d->baseResolver
        ? d->baseResolver->globalPermissions(subjectId)
        : GlobalPermissions{};
}

ResourceAccessDetails IntercomLayoutAccessResolver::accessDetails(
    const QnUuid& subjectId,
    const QnResourcePtr& resource,
    nx::vms::api::AccessRight accessRight) const
{
    if (!d->baseResolver || !d->resourcePool || !NX_ASSERT(resource))
        return {};

    const bool hasAccess = accessRights(subjectId, resource).testFlag(accessRight);
    if (!hasAccess)
        return {};

    QnResourcePtr target = resource;
    if (const auto layout = resource.objectCast<QnLayoutResource>())
    {
        if (const auto parentResource = layout->getParentResource())
        {
            if (nx::vms::common::isIntercomOnIntercomLayout(parentResource, layout))
                target = parentResource;
        }
    }

    return d->baseResolver->accessDetails(subjectId, target, accessRight);
}

// -----------------------------------------------------------------------------------------------
// IntercomLayoutAccessResolver::Private

IntercomLayoutAccessResolver::Private::Private(
    IntercomLayoutAccessResolver* q,
    AbstractResourceAccessResolver* baseResolver,
    QnResourcePool* resourcePool)
    :
    q(q),
    baseResolver(baseResolver),
    resourcePool(resourcePool)
{
    NX_CRITICAL(baseResolver && resourcePool);

    connect(q->notifier(), &Notifier::subjectsSubscribed,
        baseResolver->notifier(), &Notifier::subscribeSubjects, Qt::DirectConnection);

    connect(q->notifier(), &Notifier::subjectsReleased,
        baseResolver->notifier(), &Notifier::releaseSubjects, Qt::DirectConnection);

    connect(baseResolver->notifier(), &Notifier::resourceAccessChanged,
        this, &Private::handleBaseAccessChanged, Qt::DirectConnection);

    connect(baseResolver->notifier(), &Notifier::resourceAccessReset,
        this, &Private::handleReset, Qt::DirectConnection);

    connect(resourcePool, &QnResourcePool::resourcesAdded,
        this, &Private::handleResourcesAdded, Qt::DirectConnection);

    connect(resourcePool, &QnResourcePool::resourcesRemoved,
        this, &Private::handleResourcesRemoved, Qt::DirectConnection);

    NX_MUTEX_LOCKER lk(&mutex);
    const auto intercoms = resourcePool->getResources<QnVirtualCameraResource>(
        &nx::vms::common::isIntercom);

    allIntercoms = {intercoms.cbegin(), intercoms.cend()};
}

ResourceAccessMap IntercomLayoutAccessResolver::Private::ensureAccessMap(
    const QnUuid& subjectId) const
{
    NX_MUTEX_LOCKER lk(&mutex);

    if (cachedAccessMaps.contains(subjectId))
        return cachedAccessMaps.value(subjectId);

    ResourceAccessMap accessMap = baseResolver->resourceAccessMap(subjectId);

    constexpr AccessRights kRequiredAccessRights = AccessRight::view | AccessRight::userInput;
    constexpr AccessRights kGrantedAccessRights = AccessRight::view;

    for (const auto& intercom: allIntercoms)
    {
        const auto accessRights = baseResolver->accessRights(subjectId, intercom);
        const auto intercomLayoutId = nx::vms::common::calculateIntercomLayoutId(intercom);
        if (accessRights.testFlags(kRequiredAccessRights))
            accessMap.emplace(intercomLayoutId, kGrantedAccessRights);
        else
            accessMap.remove(intercomLayoutId);
    }

    cachedAccessMaps.emplace(subjectId, accessMap);
    baseResolver->notifier()->subscribeSubjects({subjectId});

    NX_DEBUG(q, "Resolved and cached an access map for %1", subjectId);
    NX_VERBOSE(q, toString(accessMap, resourcePool));

    return accessMap;
}

void IntercomLayoutAccessResolver::Private::handleBaseAccessChanged(const QSet<QnUuid>& subjectIds)
{
    NX_DEBUG(q, "Base resolution changed for %1 subjects: %2", subjectIds.size(), subjectIds);
    invalidateSubjects(subjectIds);
    q->notifyAccessChanged(subjectIds);
}

void IntercomLayoutAccessResolver::Private::invalidateSubjects(const QSet<QnUuid>& subjectIds)
{
    QSet<QnUuid> affectedCachedSubjectIds;
    {
        NX_MUTEX_LOCKER lk(&mutex);

        for (const auto& subjectId: subjectIds)
        {
            if (cachedAccessMaps.remove(subjectId))
                affectedCachedSubjectIds.insert(subjectId);
        }
    }

    if (affectedCachedSubjectIds.empty())
        return;

    NX_DEBUG(q, "Cache invalidated for %1 subjects: %2",
        affectedCachedSubjectIds.size(), affectedCachedSubjectIds);

    baseResolver->notifier()->releaseSubjects(affectedCachedSubjectIds);
}

void IntercomLayoutAccessResolver::Private::handleReset()
{
    const auto affectedCachedSubjectIds = invalidateCache();
    NX_DEBUG(q, "Base resolution reset, whole cache invalidated");
    baseResolver->notifier()->releaseSubjects(affectedCachedSubjectIds);
    q->notifyAccessReset();
}

QSet<QnUuid> IntercomLayoutAccessResolver::Private::invalidateCache()
{
    NX_MUTEX_LOCKER lk(&mutex);
    const auto cachedSubjectIds = QSet<QnUuid>(
        cachedAccessMaps.keyBegin(), cachedAccessMaps.keyEnd());

    cachedAccessMaps.clear();
    return cachedSubjectIds;
}

void IntercomLayoutAccessResolver::Private::handleResourcesAdded(const QnResourceList& resources)
{
    handleIntercomsAddedOrRemoved(resources.filtered<QnVirtualCameraResource>(
        &nx::vms::common::isIntercom), /*isAdded*/ true);
}

void IntercomLayoutAccessResolver::Private::handleResourcesRemoved(const QnResourceList& resources)
{
    handleIntercomsAddedOrRemoved(resources.filtered<QnVirtualCameraResource>(
        &nx::vms::common::isIntercom), /*isAdded*/ false);
}

void IntercomLayoutAccessResolver::Private::handleIntercomsAddedOrRemoved(
    const QnVirtualCameraResourceList& intercoms, bool isAdded)
{
    if (!baseResolver || intercoms.empty())
        return;

    QSet<QnUuid> affectedCachedSubjectIds;
    {
        NX_MUTEX_LOCKER lk(&mutex);
        for (const auto& intercom: intercoms)
        {
            for (const auto& subjectId: nx::utils::keyRange(cachedAccessMaps))
            {
                if (baseResolver->accessRights(subjectId, intercom))
                    affectedCachedSubjectIds.insert(subjectId);
            }

            for (const auto& subjectId: affectedCachedSubjectIds)
                cachedAccessMaps.remove(subjectId);

            if (isAdded)
                allIntercoms.insert(intercom);
            else
                allIntercoms.remove(intercom);
        }
    }

    NX_DEBUG(q, "Intercoms %1 %2, %3", intercoms,
        isAdded ? "added" : "removed", affectedCacheToLogString(affectedCachedSubjectIds));

    baseResolver->notifier()->releaseSubjects(affectedCachedSubjectIds);
    notifyResolutionChanged(intercoms, /*knownAffectedSubjectIds*/ affectedCachedSubjectIds);
}

void IntercomLayoutAccessResolver::Private::notifyResolutionChanged(
    const QnVirtualCameraResourceList& intercoms, const QSet<QnUuid>& knownAffectedSubjectIds)
{
    const auto watchedSubjectIds = q->notifier()->watchedSubjectIds();
    QSet<QnUuid> affectedWatchedSubjectIds;

    for (const auto id: watchedSubjectIds)
    {
        if (knownAffectedSubjectIds.contains(id))
        {
            affectedWatchedSubjectIds.insert(id);
        }
        else
        {
            for (const auto& intercom: intercoms)
            {
                if (baseResolver->accessRights(id, intercom))
                {
                    affectedWatchedSubjectIds.insert(id);
                    break;
                }
            }
        }
    }

    q->notifyAccessChanged(affectedWatchedSubjectIds);
}

} // namespace nx::core::access
