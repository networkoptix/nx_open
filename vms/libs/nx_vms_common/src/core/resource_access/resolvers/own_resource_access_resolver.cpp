// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "own_resource_access_resolver.h"

#include <QtCore/QPointer>

#include <core/resource/layout_resource.h>
#include <core/resource_access/abstract_access_rights_manager.h>
#include <core/resource_access/abstract_global_permissions_watcher.h>
#include <nx/utils/log/assert.h>

namespace nx::core::access {

struct OwnResourceAccessResolver::Private
{
    const QPointer<AbstractAccessRightsManager> accessRightsManager;
    const QPointer<AbstractGlobalPermissionsWatcher> globalPermissionsWatcher;
};

OwnResourceAccessResolver::OwnResourceAccessResolver(
    AbstractAccessRightsManager* accessRightsManager,
    AbstractGlobalPermissionsWatcher* globalPermissionsWatcher,
    QObject* parent)
    :
    AbstractResourceAccessResolver(parent),
    d(new Private{accessRightsManager, globalPermissionsWatcher})
{
    NX_CRITICAL(d->accessRightsManager && d->globalPermissionsWatcher);

    connect(accessRightsManager, &AbstractAccessRightsManager::ownAccessRightsChanged,
        this, &OwnResourceAccessResolver::notifyAccessChanged, Qt::DirectConnection);

    connect(accessRightsManager, &AbstractAccessRightsManager::accessRightsReset,
        this, &OwnResourceAccessResolver::notifyAccessReset, Qt::DirectConnection);

    connect(globalPermissionsWatcher, &AbstractGlobalPermissionsWatcher::ownGlobalPermissionsChanged,
        this, [this](const QnUuid& subjectId) { notifyAccessChanged({subjectId}); },
        Qt::DirectConnection);

    connect(globalPermissionsWatcher, &AbstractGlobalPermissionsWatcher::globalPermissionsReset,
        this, &OwnResourceAccessResolver::notifyAccessReset, Qt::DirectConnection);
}

OwnResourceAccessResolver::~OwnResourceAccessResolver()
{
    // Required here for forward-declared scoped pointer destruction.
}

ResourceAccessMap OwnResourceAccessResolver::resourceAccessMap(const QnUuid& subjectId) const
{
    if (!NX_ASSERT(d->accessRightsManager) || hasAdminAccessRights(subjectId))
        return {};

    return d->accessRightsManager->ownResourceAccessMap(subjectId);
}

AccessRights OwnResourceAccessResolver::accessRights(const QnUuid& subjectId,
    const QnResourcePtr& resource) const
{
    return resource && resource->hasFlags(Qn::desktop_camera)
        ? AccessRights{}
        : base_type::accessRights(subjectId, resource);
}

GlobalPermissions OwnResourceAccessResolver::globalPermissions(const QnUuid& subjectId) const
{
    return NX_ASSERT(d->globalPermissionsWatcher)
        ? d->globalPermissionsWatcher->ownGlobalPermissions(subjectId)
        : GlobalPermissions{};
}

ResourceAccessDetails OwnResourceAccessResolver::accessDetails(
    const QnUuid& subjectId,
    const QnResourcePtr& resource,
    nx::vms::api::AccessRight accessRight) const
{
    return accessRights(subjectId, resource).testFlag(accessRight)
        ? ResourceAccessDetails({{subjectId, {resource}}})
        : ResourceAccessDetails();
}

} // namespace nx::core::access
