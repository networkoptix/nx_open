// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_resource_access_resolver.h"

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/webpage_resource.h>
#include <core/resource_access/abstract_access_rights_manager.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/utils/scoped_connections.h>
#include <nx/utils/thread/mutex.h>

using namespace nx::vms::api;

namespace nx::core::access {

namespace {

AccessRights viewOnlyAccessRights(AccessRights accessRights)
{
    return accessRights & kViewAccessRights;
}

} // namespace

// ------------------------------------------------------------------------------------------------
// AbstractResourceAccessResolver

AbstractResourceAccessResolver::AbstractResourceAccessResolver(QObject* parent):
    base_type(parent),
    m_notifier(new Notifier())
{
}

AbstractResourceAccessResolver::~AbstractResourceAccessResolver()
{
    // Required here for forward-declared scoped pointer destruction.
}

AccessRights AbstractResourceAccessResolver::accessRights(
    const QnUuid& subjectId,
    const QnResourcePtr& resource) const
{
    if (!NX_ASSERT(resource) || !resource->resourcePool() || resource->hasFlags(Qn::removed))
        return {};

    if (hasFullAccessRights(subjectId))
    {
        const auto layout = resource.objectCast<QnLayoutResource>();
        if (layout && layout->isShared())
            return kFullAccessRights;
    }

    const auto accessMap = resourceAccessMap(subjectId);

    if (resource->hasFlags(Qn::desktop_camera))
        return accessMap.value(resource->getId());

    if (const auto camera = resource.objectCast<QnSecurityCamResource>())
        return accessMap.value(camera->getId()) | accessMap.value(kAllDevicesGroupId);

    if (const auto layout = resource.objectCast<QnLayoutResource>())
        return accessMap.value(layout->getId());

    if (const auto webPage = resource.objectCast<QnWebPageResource>())
    {
        return viewOnlyAccessRights(
            accessMap.value(webPage->getId()) | accessMap.value(kAllWebPagesGroupId));
    }

    if (const auto server = resource.objectCast<QnMediaServerResource>())
    {
        return viewOnlyAccessRights(
            accessMap.value(server->getId()) | accessMap.value(kAllServersGroupId));
    }

    if (const auto videowall = resource.objectCast<QnVideoWallResource>())
        return accessMap.value(videowall->getId()) | accessMap.value(kAllVideoWallsGroupId);

    return {};
}

bool AbstractResourceAccessResolver::hasFullAccessRights(const QnUuid& subjectId) const
{
    return globalPermissions(subjectId).testFlag(GlobalPermission::powerUser);
}

AbstractResourceAccessResolver::Notifier* AbstractResourceAccessResolver::notifier() const
{
    return m_notifier.get();
}

void AbstractResourceAccessResolver::notifyAccessChanged(const QSet<QnUuid>& subjectIds)
{
    if (const auto ids = subjectIds & notifier()->watchedSubjectIds(); !ids.empty())
    {
        NX_VERBOSE(this, "Notifying about a change of %1 watched subjects: %2", ids.size(), ids);
        emit notifier()->resourceAccessChanged(ids);
    }
}

void AbstractResourceAccessResolver::notifyAccessReset()
{
    NX_DEBUG(this, "Notifying about a reset");
    emit notifier()->resourceAccessReset();
}

QString AbstractResourceAccessResolver::toLogString(
    const QnUuid& resourceId, QnResourcePool* resourcePool)
{
    if (resourceId.isNull())
        return "none";

    const auto resource = resourcePool->getResourceById(resourceId);

    return resource
        ? nx::toString(resource)
        : NX_FMT("%1 (not in the pool)", resourceId).toQString();
}

QString AbstractResourceAccessResolver::affectedCacheToLogString(
    const QSet<QnUuid>& affectedSubjectIds)
{
    if (affectedSubjectIds.empty())
        return "cached subjects weren't affected";

    return nx::format("cache invalidated for %2 affected subjects: %3",
        affectedSubjectIds.size(), affectedSubjectIds);
}

// ------------------------------------------------------------------------------------------------
// AbstractResourceAccessResolver::Notifier

struct AbstractResourceAccessResolver::Notifier::Private
{
    QnCounterHash<QnUuid> watchedSubjectCounters;
    QSet<QnUuid> watchedSubjectIds;
    mutable nx::Mutex mutex;
};

AbstractResourceAccessResolver::Notifier::Notifier(QObject* parent):
    base_type(parent),
    d(new Private{})
{
}

AbstractResourceAccessResolver::Notifier::~Notifier()
{
    // Required here for forward-declared scoped pointer destruction.
}

void AbstractResourceAccessResolver::Notifier::subscribeSubjects(const QSet<QnUuid>& subjectIds)
{
    if (subjectIds.empty())
        return;

    NX_MUTEX_LOCKER lk(&d->mutex);
    for (const auto& subjectId: subjectIds)
    {
        if (d->watchedSubjectCounters.insert(subjectId))
            d->watchedSubjectIds.insert(subjectId);
    }

    NX_VERBOSE(this, "Added a subscription for %1 subjects: %2; currently watching %3 subjects: %4",
        subjectIds.size(), subjectIds, d->watchedSubjectIds.size(), d->watchedSubjectIds);

    lk.unlock();
    emit subjectsSubscribed(subjectIds);
}

void AbstractResourceAccessResolver::Notifier::releaseSubjects(const QSet<QnUuid>& subjectIds)
{
    if (subjectIds.empty())
        return;

    NX_MUTEX_LOCKER lk(&d->mutex);
    for (const auto& subjectId: subjectIds)
    {
        if (d->watchedSubjectCounters.remove(subjectId))
            d->watchedSubjectIds.remove(subjectId);
    }

    NX_VERBOSE(this, "Removed a subscription for %1 subjects: %2; currently watching %3 subjects: %4",
        subjectIds.size(), subjectIds, d->watchedSubjectIds.size(), d->watchedSubjectIds);

    lk.unlock();
    emit subjectsReleased(subjectIds);
}

QSet<QnUuid> AbstractResourceAccessResolver::Notifier::watchedSubjectIds() const
{
    NX_MUTEX_LOCKER lk(&d->mutex);
    return d->watchedSubjectIds;
}

} // namespace nx::core::access
