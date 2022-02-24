// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "permissions_resource_access_provider.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource_access/global_permissions_manager.h>
#include <core/resource_management/user_roles_manager.h>

#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>
#include <common/common_module.h>
#include <core/resource_access/resource_access_subject.h>

namespace nx::core::access {

PermissionsResourceAccessProvider::PermissionsResourceAccessProvider(
    Mode mode,
    QObject* parent)
    :
    base_type(mode, parent)
{
    if (mode == Mode::cached)
    {
        connect(globalPermissionsManager(), &QnGlobalPermissionsManager::globalPermissionsChanged,
            this, &PermissionsResourceAccessProvider::updateAccessBySubject);
    }
}

PermissionsResourceAccessProvider::~PermissionsResourceAccessProvider()
{
}

Source PermissionsResourceAccessProvider::baseSource() const
{
    return Source::permissions;
}

bool PermissionsResourceAccessProvider::calculateAccess(const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource,
    GlobalPermissions globalPermissions) const
{
    NX_ASSERT(acceptable(subject, resource));
    if (!acceptable(subject, resource))
        return false;

    if (resource == subject.user())
        return true;

    // Nobody has access to desktop cameras by default.
    if (resource->hasFlags(Qn::desktop_camera))
        return false;

    auto requiredPermission = GlobalPermission::admin;
    if (isMediaResource(resource))
        requiredPermission = GlobalPermission::accessAllMedia;
    else if (resource->hasFlags(Qn::videowall))
        requiredPermission = GlobalPermission::controlVideowall;
    else if (isLayout(resource) && subject.user() && resource->getParentId() == subject.id())
        requiredPermission = {};

    return QnGlobalPermissionsManager::containsPermission(globalPermissions, requiredPermission);
}

void PermissionsResourceAccessProvider::handleResourceAdded(const QnResourcePtr& resource)
{
    NX_ASSERT(mode() == Mode::cached);

    base_type::handleResourceAdded(resource);
    if (isLayout(resource))
    {
        connect(resource, &QnResource::parentIdChanged, this,
            &PermissionsResourceAccessProvider::updateAccessToResource);
    }

    if (resource->hasFlags(Qn::desktop_camera))
    {
        connect(resource, &QnResource::nameChanged, this,
            &PermissionsResourceAccessProvider::updateAccessToResource);
    }
}

} // namespace nx::core::access
