// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_resource_access_resolver.h"

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/webpage_resource.h>
#include <core/resource_access/abstract_access_rights_manager.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/utils/scoped_connections.h>
#include <nx/utils/thread/mutex.h>

using namespace nx::vms::api;

namespace nx::core::access {

namespace {

AccessRights cameraAccessRights(AccessRights accessRights)
{
    return accessRights & ~(AccessRights(AccessRight::controlVideowall));
}

AccessRights viewOnlyAccessRights(AccessRights accessRights)
{
    return accessRights & AccessRights(AccessRight::view);
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

    const bool isDesktopCamera = resource->hasFlags(Qn::desktop_camera);

    if (hasAdminAccessRights(subjectId) && !isDesktopCamera)
        return kAdminAccessRights;

    const auto accessMap = resourceAccessMap(subjectId);

    const auto anyResourceAccessRights = isDesktopCamera
        ? AccessRights{}
        : accessMap.value(AbstractAccessRightsManager::kAnyResourceId);

    if (resource.objectCast<QnSecurityCamResource>())
        return cameraAccessRights(accessMap.value(resource->getId()) | anyResourceAccessRights);

    if (resource.objectCast<QnLayoutResource>())
        return cameraAccessRights(accessMap.value(resource->getId()));

    if (resource.objectCast<QnWebPageResource>() || resource.objectCast<QnMediaServerResource>())
        return viewOnlyAccessRights(accessMap.value(resource->getId()) | anyResourceAccessRights);

    if (resource.objectCast<QnVideoWallResource>())
    {
        const auto anyVideoWallAccessRights =
            anyResourceAccessRights.testFlag(AccessRight::controlVideowall)
                ? anyResourceAccessRights
                : AccessRights();

        return accessMap.value(resource->getId()) | anyVideoWallAccessRights;
    }

    return {};
}

bool AbstractResourceAccessResolver::hasAdminAccessRights(const QnUuid& subjectId) const
{
    return globalPermissions(subjectId).testFlag(GlobalPermission::admin);
}

AbstractResourceAccessResolver::Notifier* AbstractResourceAccessResolver::notifier() const
{
    return m_notifier.get();
}

void AbstractResourceAccessResolver::notifyAccessChanged(const QSet<QnUuid>& subjectIds)
{
    if (const auto ids = subjectIds & notifier()->watchedSubjectIds(); !ids.empty())
    {
        NX_DEBUG(this, "Notifying about a change of %1 watched subjects: %2", ids.size(), ids);
        emit notifier()->resourceAccessChanged(ids);
    }
}

void AbstractResourceAccessResolver::notifyAccessReset()
{
    NX_DEBUG(this, "Notifying about a reset");
    emit notifier()->resourceAccessReset();
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
