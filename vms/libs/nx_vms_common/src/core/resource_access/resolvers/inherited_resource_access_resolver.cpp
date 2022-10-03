// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "inherited_resource_access_resolver.h"

#include <QtCore/QPointer>

#include <core/resource_access/subject_hierarchy.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/utils/range_adapters.h>
#include <nx/utils/thread/mutex.h>

namespace nx::core::access {

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
    QSet<QnUuid> watchedSubjectParents;

    ResourceAccessMap resourceAccessMapUnsafe(
        const QnUuid& subjectId, QSet<QnUuid>& visitedSubjectIds) const;

    QSet<QnUuid> invalidateCache(); //< Returns all subject ids that were cached.

    mutable QHash<QnUuid, ResourceAccessMap> cachedAccessMaps;
    mutable nx::Mutex mutex;
    mutable nx::Mutex watchedParentsMutex;

private:
    void handleBaseAccessChanged(const QSet<QnUuid>& subjectIds);
    void handleReset();

    void handleSubjectHierarchyChanged(
        const QSet<QnUuid>& added,
        const QSet<QnUuid>& removed,
        const QSet<QnUuid>& groupsWithChangedMembers,
        const QSet<QnUuid>& subjectsWithChangedParents);

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

nx::vms::api::GlobalPermissions InheritedResourceAccessResolver::globalPermissions(const QnUuid& subjectId) const
{
    auto result = d->baseResolver
        ? d->baseResolver->globalPermissions(subjectId)
        : nx::vms::api::GlobalPermissions{};

    if (!d->subjectHierarchy)
        return result;

    for (const auto& parentId: d->subjectHierarchy->directParents(subjectId))
        result |= globalPermissions(parentId);

    return result;
}

ResourceAccessMap InheritedResourceAccessResolver::resourceAccessMap(const QnUuid& subjectId) const
{
    if (!d->subjectHierarchy || !d->baseResolver)
        return {};

    NX_MUTEX_LOCKER lk(&d->mutex);

    QSet<QnUuid> visitedSubjectIds;
    return d->resourceAccessMapUnsafe(subjectId, visitedSubjectIds);
}

ResourceAccessDetails InheritedResourceAccessResolver::accessDetails(
    const QnUuid& subjectId,
    const QnResourcePtr& resource,
    nx::vms::api::AccessRight accessRight) const
{
    if (!d->subjectHierarchy || !d->baseResolver)
        return {};

    auto details = d->baseResolver->accessDetails(subjectId, resource, accessRight);

    for (const auto& parentId: d->subjectHierarchy->directParents(subjectId))
        details += accessDetails(parentId, resource, accessRight);

    return details;
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

ResourceAccessMap InheritedResourceAccessResolver::Private::resourceAccessMapUnsafe(
    const QnUuid& subjectId, QSet<QnUuid>& visitedSubjectIds) const
{
    // Both optimization and circular dependency protection.
    if (visitedSubjectIds.contains(subjectId))
        return cachedAccessMaps.value(subjectId);

    visitedSubjectIds.insert(subjectId);

    if (cachedAccessMaps.contains(subjectId))
        return cachedAccessMaps.value(subjectId);

    ResourceAccessMap& cachedAccessMapRef = cachedAccessMaps[subjectId];
    cachedAccessMapRef = baseResolver->resourceAccessMap(subjectId);

    ResourceAccessMap inheritedAccessMap;
    const auto parents = subjectHierarchy->directParents(subjectId);

    for (const auto& parent: parents)
        inheritedAccessMap += resourceAccessMapUnsafe(parent, visitedSubjectIds);

    baseResolver->notifier()->subscribeSubjects({subjectId});
    cachedAccessMapRef += inheritedAccessMap;

    NX_DEBUG(q, "Resolved and cached an access map for %1", subjectId);
    NX_VERBOSE(q, cachedAccessMapRef);

    return cachedAccessMapRef;
}

QSet<QnUuid> InheritedResourceAccessResolver::Private::invalidateCache()
{
    NX_MUTEX_LOCKER lk(&mutex);
    const auto cachedSubjectIds = QSet<QnUuid>(
        cachedAccessMaps.keyBegin(), cachedAccessMaps.keyEnd());

    cachedAccessMaps.clear();
    return cachedSubjectIds;
}

void InheritedResourceAccessResolver::Private::handleBaseAccessChanged(
    const QSet<QnUuid>& subjectIds)
{
    NX_DEBUG(q, "Base resolution changed for %1 subjects: %2", subjectIds.size(), subjectIds);
    QSet<QnUuid> affectedCachedSubjectIds;

    {
        NX_MUTEX_LOCKER lk(&mutex);

        for (const auto& cachedSubjectId: nx::utils::keyRange(cachedAccessMaps))
        {
            if (subjectIds.contains(cachedSubjectId)
                || subjectHierarchy->isRecursiveMember(cachedSubjectId, subjectIds))
            {
                affectedCachedSubjectIds.insert(cachedSubjectId);
            }
        }

        for (const auto& subjectId: affectedCachedSubjectIds)
            cachedAccessMaps.remove(subjectId);
    }

    NX_DEBUG(q, "Cache invalidated for %1 subjects: %2",
        affectedCachedSubjectIds.size(), affectedCachedSubjectIds);

    const auto watchedSubjectIds = q->notifier()->watchedSubjectIds();
    QSet<QnUuid> affectedWatchedSubjectIds;

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
    const QSet<QnUuid>& /*added*/,
    const QSet<QnUuid>& /*removed*/,
    const QSet<QnUuid>& /*groupsWithChangedMembers*/,
    const QSet<QnUuid>& subjectsWithChangedParents)
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
