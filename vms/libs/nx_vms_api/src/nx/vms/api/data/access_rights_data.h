// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <optional>
#include <vector>

#include <nx/utils/uuid.h>
#include <nx/vms/api/types/access_rights_types.h>

#include "check_resource_exists.h"
#include "data_macros.h"

namespace nx::vms::api {

struct NX_VMS_API AccessRightsData
{
    QnUuid userId;
    std::map<QnUuid, AccessRights> resourceRights;

    /** Used by ...Model::toDbTypes() and transaction-description-modify checkers. */
    CheckResourceExists checkResourceExists = CheckResourceExists::yes; /**<%apidoc[unused] */

    bool operator==(const AccessRightsData& other) const = default;
    QnUuid getId() const { return userId; }
};
#define AccessRightsData_Fields (userId)(resourceRights)
NX_VMS_API_DECLARE_STRUCT_AND_LIST(AccessRightsData)

// Special resource group ids.
NX_VMS_API extern const QnUuid kAllDevicesGroupId;
NX_VMS_API extern const QnUuid kAllWebPagesGroupId;
NX_VMS_API extern const QnUuid kAllServersGroupId;
NX_VMS_API extern const QnUuid kAllVideowallsGroupId;

NX_VMS_API std::optional<SpecialResourceGroup> specialResourceGroup(const QnUuid& id);
NX_VMS_API QnUuid specialResourceGroupId(SpecialResourceGroup group);

NX_VMS_API std::map<QnUuid, AccessRights> migrateAccessRights(
    GlobalPermissions permissions, const std::vector<QnUuid>& accessibleResources);

} // namespace nx::vms::api
