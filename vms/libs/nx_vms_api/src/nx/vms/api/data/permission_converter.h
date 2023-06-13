// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <optional>
#include <vector>

#include <nx/utils/uuid.h>
#include <nx/vms/api/types/access_rights_types.h>

namespace nx::vms::api {

NX_VMS_API std::map<QnUuid, AccessRights> migrateAccessRights(
    GlobalPermissions* permissions, const std::vector<QnUuid>& accessibleResources);

namespace PermissionConverter
{
    NX_VMS_API void extractFromResourceAccessRights(
        const std::map<QnUuid, AccessRights>& resourceAccessRights,
        GlobalPermissions* permissions,
        std::optional<std::vector<QnUuid>>* accessibleResources);
};

} // namespace nx::vms::api
