// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "inherited_resource_access_resolver.h"

#include <QtCore/QPointer>

#include <core/resource_access/subject_hierarchy.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/utils/range_adapters.h>
#include <nx/utils/thread/mutex.h>

namespace nx::core::access {

using namespace nx::vms::api;

class InheritedResourceAccessResolver::Private: public QObject
{
    InheritedResourceAccessResolver* const q;

public:
    explicit Private(
        InheritedResourceAccessResolver* q,
        AbstractResourceAccessResolver* baseResolver,
        SubjectHierarchy* subjectHierarchy);

    const QPointer<AbstractResourceAccessResolver> baseResolver;
    const QPointer<SubjectHierarchy> subjectHierarchy;
    QSet<nx::Uuid> watchedSubjectParents;

    struct ResourceAccessData
    {
        ResourceAccessMap accessMap;
        std::optional<AccessRights> availableAccessRights;
    };

    ResourceAccessData ensureResourceAccessDataUnsafe(
        const nx::Uuid& subjectId, QSet<nx::Uuid>& visitedSubjectIds) const;

    QSet<nx::Uuid> invalidateCache(); //< Returns all subject ids that were cached.

    GlobalPermissions globalPermissions(
        const nx::Uuid& subjectId, QSet<nx::Uuid>& visitedSubjectIds) const;

    ResourceAccessDetails accessDetails(
        const nx::Uuid& subjectId,
        const QnResourcePtr& resource,
        nx::vms::api::AccessRight accessRight,
        QSet<nx::Uuid>& visitedSubjectIds) const;

    mutable QHash<nx::Uuid, ResourceAccessData> cachedAccessData;
    mutable nx::Mutex mutex;
    mutable nx::Mutex watchedParentsMutex;

private:
    void handleBaseAccessChanged(const QSet<nx::Uuid>& subjectIds);
    void handleReset();

    void handleSubjectHierarchyChanged(
        const QSet<nx::Uuid>& added,
        const QSet<nx::Uuid>& removed,
        const QSet<nx::Uuid>& groupsWithChangedMembers,
        const QSet<nx::Uuid>& subjectsWithChangedParents);

    void updateWatchedSubset();
};

// -----------------------------------------------------------------------------------------------
// InheritedResourceAccessResolver

InheritedResourceAccessResolver::InheritedResourceAccessResolver(
    AbstractResourceAccessResolver* baseResolver,
    SubjectHierarchy* subjectHierarchy,
    QObject* parent)
    :
    base_type(parent),
    d(new Private(this, baseResolver, subjectHierarchy))
{
}

InheritedResourceAccessResolver::~InheritedResourceAccessResolver()
{
    // Required here for forward-declared scoped pointer destruction.
}

GlobalPermissions InheritedResourceAccessResolver::globalPermissions(const nx::Uuid& subjectId) const
{
    QSet<nx::Uuid> visitedSubjectIds;
    return d->globalPermissions(subjectId, visitedSubjectIds);
}

AccessRights InheritedResourceAccessResolver::availableAccessRights(const nx::Uuid& subjectId) const
{
    if (!d->subjectHierarchy || !d->baseResolver)
        return {};

    NX_MUTEX_LOCKER lk(&d->mutex);

    QSet<nx::Uuid> visitedSubjectIds;
    const auto data = d->ensureResourceAccessDataUnsafe(subjectId, visitedSubjectIds);

    if (data.availableAccessRights)
        return *data.availableAccessRights;

    const auto result = std::accumulate(
        data.accessMap.constKeyValueBegin(), data.accessMap.constKeyValueEnd(),
        AccessRights{},
        [](AccessRights sum, const std::pair<nx::Uuid, AccessRights>& item)
        {
            return sum | item.second;
        });

    d->cachedAccessData[subjectId].availableAccessRights = result;
    return result;
}

ResourceAccessMap InheritedResourceAccessResolver::resourceAccessMap(const nx::Uuid& subjectId) const
{
    if (!d->subjectHierarchy || !d->baseResolver)
        return {};

    NX_MUTEX_LOCKER lk(&d->mutex);

    QSet<nx::Uuid> visitedSubjectIds;
    return d->ensureResourceAccessDataUnsafe(subjectId, visitedSubjectIds).accessMap;
}

ResourceAccessDetails InheritedResourceAccessResolver::accessDetails(
    const nx::Uuid& subjectId,
    const QnResourcePtr& resource,
    nx::vms::api::AccessRight accessRight) const
{
    if (!d->subjectHierarchy || !d->baseResolver)
        return {};

    QSet<nx::Uuid> visitedSubjectIds;
    return d->accessDetails(subjectId, resource, accessRight, visitedSubjectIds);
}

// -----------------------------------------------------------------------------------------------
// InheritedResourceAccessResolver::Private

InheritedResourceAccessResolver::Private::Private(
    InheritedResourceAccessResolver* q,
    AbstractResourceAccessResolver* baseResolver,
    SubjectHierarchy* subjectHierarchy)
    :
    q(q),
    baseResolver(baseResolver),
    subjectHierarchy(subjectHierarchy)
{
    NX_CRITICAL(baseResolver && subjectHierarchy);

    connect(q->notifier(), &Notifier::subjectsSubscribed,
        baseResolver->notifier(), &Notifier::subscribeSubjects, Qt::DirectConnection);

    connect(q->notifier(), &Notifier::subjectsReleased,
        baseResolver->notifier(), &Notifier::releaseSubjects, Qt::DirectConnection);

    connect(baseResolver->notifier(), &Notifier::resourceAccessChanged,
        this, &Private::handleBaseAccessChanged, Qt::DirectConnection);

    connect(baseResolver->notifier(), &Notifier::resourceAccessReset,
        this, &Private::handleReset, Qt::DirectConnection);

    connect(q->notifier(), &Notifier::subjectsSubscribed,
        this, &Private::updateWatchedSubset, Qt::DirectConnection);

    connect(q->notifier(), &Notifier::subjectsReleased,
        this, &Private::updateWatchedSubset, Qt::DirectConnection);

    connect(subjectHierarchy, &SubjectHierarchy::changed,
        this, &Private::handleSubjectHierarchyChanged, Qt::DirectConnection);
}

InheritedResourceAccessResolver::Private::ResourceAccessData
    InheritedResourceAccessResolver::Private::ensureResourceAccessDataUnsafe(
        const nx::Uuid& subjectId, QSet<nx::Uuid>& visitedSubjectIds) const
{
    // Both optimization and circular dependency protection.
    if (visitedSubjectIds.contains(subjectId))
        return cachedAccessData.value(subjectId);

    visitedSubjectIds.insert(subjectId);

    if (cachedAccessData.contains(subjectId))
        return cachedAccessData.value(subjectId);

    cachedAccessData[subjectId].accessMap = baseResolver->resourceAccessMap(subjectId);

    ResourceAccessMap inheritedAccessMap;
    const auto parents = subjectHierarchy->directParents(subjectId);

    for (const auto& parent: parents)
        inheritedAccessMap += ensureResourceAccessDataUnsafe(parent, visitedSubjectIds).accessMap;

    baseResolver->notifier()->subscribeSubjects({subjectId});

    ResourceAccessData& cachedAccessDataRef = cachedAccessData[subjectId];
    cachedAccessDataRef.accessMap += inheritedAccessMap;
    cachedAccessDataRef.accessMap.remove(nx::Uuid{}); //< For compatibility with intermediate version.

    NX_DEBUG(q, "Resolved and cached an access map for %1", subjectId);
    NX_VERBOSE(q, toString(cachedAccessDataRef.accessMap));

    return cachedAccessDataRef;
}

QSet<nx::Uuid> InheritedResourceAccessResolver::Private::invalidateCache()
{
    NX_MUTEX_LOCKER lk(&mutex);
    const auto cachedSubjectIds = QSet<nx::Uuid>(
        cachedAccessData.keyBegin(), cachedAccessData.keyEnd());

    cachedAccessData.clear();
    return cachedSubjectIds;
}

GlobalPermissions InheritedResourceAccessResolver::Private::globalPermissions(
    const nx::Uuid& subjectId, QSet<nx::Uuid>& visitedSubjectIds) const
{
    if (visitedSubjectIds.contains(subjectId))
        return {};

    visitedSubjectIds.insert(subjectId);

    auto result = baseResolver
        ? baseResolver->globalPermissions(subjectId)
        : nx::vms::api::GlobalPermissions{};

    if (!subjectHierarchy)
        return result;

    // `subjectHierarchy->directParents(subjectId)` may operate on a changed hierarchy state
    // between recursive calls to `globalPermissions`. Hence, it may introduce a data race
    // regardless of the mutex being locked when `directParents` is called.
    // Moreover, locking and unlocking mutex is expensive. And here it will be done several times in a row.
    for (const auto& parentId: subjectHierarchy->directParents(subjectId))
        result |= globalPermissions(parentId, visitedSubjectIds);

    return result;
}

ResourceAccessDetails InheritedResourceAccessResolver::Private::accessDetails(
    const nx::Uuid& subjectId,
    const QnResourcePtr& resource,
    nx::vms::api::AccessRight accessRight,
    QSet<nx::Uuid>& visitedSubjectIds) const
{
    if (visitedSubjectIds.contains(subjectId))
        return {};

    visitedSubjectIds.insert(subjectId);

    auto details = baseResolver->accessDetails(subjectId, resource, accessRight);

    for (const auto& parentId: subjectHierarchy->directParents(subjectId))
        details += accessDetails(parentId, resource, accessRight, visitedSubjectIds);

    return details;
}

void InheritedResourceAccessResolver::Private::handleBaseAccessChanged(
    const QSet<nx::Uuid>& subjectIds)
{
    NX_DEBUG(q, "Base resolution changed for %1 subjects: %2", subjectIds.size(), subjectIds);
    QSet<nx::Uuid> affectedCachedSubjectIds;

    {
        NX_MUTEX_LOCKER lk(&mutex);

        for (const auto& cachedSubjectId: nx::utils::keyRange(cachedAccessData))
        {
            if (subjectIds.contains(cachedSubjectId)
                || subjectHierarchy->isRecursiveMember(cachedSubjectId, subjectIds))
            {
                affectedCachedSubjectIds.insert(cachedSubjectId);
            }
        }

        for (const auto& subjectId: affectedCachedSubjectIds)
            cachedAccessData.remove(subjectId);
    }

    NX_DEBUG(q, "Cache invalidated for %1 subjects: %2",
        affectedCachedSubjectIds.size(), affectedCachedSubjectIds);

    const auto watchedSubjectIds = q->notifier()->watchedSubjectIds();
    QSet<nx::Uuid> affectedWatchedSubjectIds;

    for (const auto subjectId: watchedSubjectIds)
    {
        if (subjectIds.contains(subjectId)
            || affectedCachedSubjectIds.contains(subjectId)
            || subjectHierarchy->isRecursiveMember(subjectId, subjectIds))
        {
            affectedWatchedSubjectIds.insert(subjectId);
        }
    }

    if (!affectedCachedSubjectIds.empty())
        baseResolver->notifier()->releaseSubjects(affectedCachedSubjectIds);

    if (!affectedWatchedSubjectIds.empty())
        q->notifyAccessChanged(affectedWatchedSubjectIds);
}

void InheritedResourceAccessResolver::Private::handleReset()
{
    const auto affectedCachedSubjectIds = invalidateCache();
    NX_DEBUG(q, "Base resolution reset, whole cache invalidated");

    if (!affectedCachedSubjectIds.empty())
        baseResolver->notifier()->releaseSubjects(affectedCachedSubjectIds);

    q->notifyAccessReset();
}

void InheritedResourceAccessResolver::Private::handleSubjectHierarchyChanged(
    const QSet<nx::Uuid>& /*added*/,
    const QSet<nx::Uuid>& /*removed*/,
    const QSet<nx::Uuid>& /*groupsWithChangedMembers*/,
    const QSet<nx::Uuid>& subjectsWithChangedParents)
{
    if (subjectsWithChangedParents.empty())
        return;

    NX_VERBOSE(q, "Subject hierarchy changed, subjects with changed parents: %1",
        subjectsWithChangedParents);

    handleBaseAccessChanged(subjectsWithChangedParents);
    updateWatchedSubset();
}

void InheritedResourceAccessResolver::Private::updateWatchedSubset()
{
    if (!subjectHierarchy || !baseResolver)
        return;

    const auto watchedSubjects = q->notifier()->watchedSubjectIds();
    const auto recursiveParents = subjectHierarchy->recursiveParents(watchedSubjects);

    NX_MUTEX_LOCKER lk(&watchedParentsMutex);

    const auto toAdd = recursiveParents - watchedSubjectParents;
    const auto toRemove = watchedSubjectParents - recursiveParents;

    if (toAdd.empty() && toRemove.empty())
        return;

    NX_VERBOSE(q, "Updating watched subset; additional watched parents: %1, "
        "stop watching: %2, start watching: %3", watchedSubjectParents, toRemove, toAdd);

    watchedSubjectParents = recursiveParents;
    baseResolver->notifier()->releaseSubjects(toRemove);
    baseResolver->notifier()->subscribeSubjects(toAdd);
}

} // namespace nx::core::access
