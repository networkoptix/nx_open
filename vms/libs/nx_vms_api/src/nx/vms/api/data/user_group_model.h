// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <optional>
#include <tuple>

#include <nx/fusion/model_functions_fwd.h>

#include "user_data.h"
#include "user_external_id.h"
#include "user_group_data.h"

namespace nx::vms::api {

/**%apidoc User Group information object. */
struct NX_VMS_API UserGroupModel
{
    nx::Uuid id;

    /**%apidoc
     * %example User Group 1
     */
    QString name;

    /**%apidoc[opt] */
    QString description;

    /**%apidoc[opt]
     * Only local User Groups are supposed to be created by the API.
     * %value local This Group is managed by VMS.
     * %value ldap This Group is imported from LDAP Server.
     * %value[unused] cloud Unsupported.
     * %value[unused] temporaryLocal Unsupported.
     */
    UserType type = UserType::local;

    /**%apidoc[readonly] External identification data (currently used for LDAP only). */
    std::optional<UserExternalIdModel> externalId;

    /**%apidoc[opt] */
    GlobalPermissions permissions = GlobalPermission::none;

    /**%apidoc[opt] List of User Groups to inherit permissions. */
    std::vector<nx::Uuid> parentGroupIds;

    /**%apidoc[opt]
     * Access rights per Resource (can be obtained from `/rest/v{3-}/devices`,
     * `/rest/v{3-}/servers`, etc.) or Resource Group (can be obtained from
     * `/rest/v{3-}/resourceGroups`).
     */
    std::map<nx::Uuid, AccessRights> resourceAccessRights;

    /**%apidoc[readonly] */
    nx::vms::api::UserAttributes attributes{};

    bool operator==(const UserGroupModel& other) const = default;

    using DbReadTypes = std::tuple<UserGroupData>;
    using DbUpdateTypes = std::tuple<UserGroupData>;
    using DbListTypes = std::tuple<UserGroupDataList>;

    DbUpdateTypes toDbTypes() &&;
    static std::vector<UserGroupModel> fromDbTypes(DbListTypes data);
};
#define UserGroupModel_Fields \
    (id) \
    (name) \
    (description) \
    (type) \
    (permissions) \
    (parentGroupIds) \
    (resourceAccessRights) \
    (attributes) \
    (externalId)
QN_FUSION_DECLARE_FUNCTIONS(UserGroupModel, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(UserGroupModel, UserGroupModel_Fields)

} // namespace nx::vms::api
