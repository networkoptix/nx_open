// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>
#include <vector>

#include <nx/utils/uuid.h>

#include "access_rights_data.h"

namespace nx::vms::api {

namespace PermissionConverter
{
    NX_VMS_API AccessRightsData accessRights(
        GlobalPermissions* permissions,
        const QnUuid& id,
        const std::optional<std::vector<QnUuid>>& accessibleResources);

    NX_VMS_API void extractFromResourceAccessRights(
        const std::vector<AccessRightsData>& allAccessRights,
        const QnUuid& id,
        GlobalPermissions* permissions,
        std::optional<std::vector<QnUuid>>* accessibleResources);
};

} // namespace nx::vms::api
