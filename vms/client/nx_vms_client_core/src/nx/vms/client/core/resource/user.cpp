// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user.h"

#include <nx/vms/api/data/global_permission_deprecated.h>

using namespace nx::vms::api;

namespace nx::vms::client::core {

UserResource::UserResource(UserModelV1 data):
    QnUserResource(data.type, data.externalId.value_or(UserExternalIdModel()))
{
    setIdUnsafe(data.id);
    setName(data.name);
    setEnabled(data.isEnabled);
    setEmail(data.email);
    setFullName(data.fullName);

    GlobalPermissions permissions;
    std::vector<nx::Uuid> groupIds;
    std::map<nx::Uuid, AccessRights> resourceAccessRights;
    std::tie(permissions, groupIds, resourceAccessRights) =
        migrateAccessRights(
            data.permissions,
            data.accessibleResources.value_or(std::vector<nx::Uuid>()),
            data.isOwner);

    if (!data.userRoleId.isNull())
        groupIds.push_back(data.userRoleId);

    setRawPermissions(permissions);
    setGroupIds(groupIds);
    setResourceAccessRights(resourceAccessRights);

    m_overwrittenData = std::move(data);
}

UserResource::UserResource(UserModelV3 data):
    QnUserResource(data.type, data.externalId.value_or(UserExternalIdModel()))
{
    setIdUnsafe(data.id);
    setName(data.name);
    setEnabled(data.isEnabled);
    setEmail(data.email);
    setFullName(data.fullName);
    setAttributes(data.attributes);
    setRawPermissions(data.permissions);
    setGroupIds(data.groupIds);
    setResourceAccessRights(data.resourceAccessRights);
}

void UserResource::updateInternal(const QnResourcePtr& source, NotifierList& notifiers)
{
    // Ignore user data changes in the compatibility mode connection.
    if (m_overwrittenData)
        return;

    base_type::updateInternal(source, notifiers);
}

} // namespace nx::vms::client::core
