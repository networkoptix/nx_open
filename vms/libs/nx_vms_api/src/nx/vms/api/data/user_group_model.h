// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <optional>
#include <tuple>

#include <nx/fusion/model_functions_fwd.h>

#include "access_rights_data.h"
#include "user_data.h"
#include "user_group_data.h"
#include "user_external_id.h"

namespace nx::vms::api {

/**%apidoc User Group information object. */
struct NX_VMS_API UserGroupModel
{
    QnUuid id;

    /**%apidoc
     * %example User Group 1
     */
    QString name;

    /**%apidoc[opt] */
    QString description;

    /**%apidoc[opt] Only local User Groups are supposed to be created by the API. */
    UserType type = UserType::local;

    /**%apidoc[readonly] External identification data (currently used for LDAP only). */
    std::optional<UserExternalId> externalId;

    /**%apidoc[opt] */
    GlobalPermissions permissions;

    /**%apidoc[opt] List of User Groups to inherit permissions. */
    std::vector<QnUuid> parentGroupIds;

    /**%apidoc
     * Resource ids (can be obtained from `GET /rest/v{3-}/devices`, `GET /rest/v{3-}/servers`,
     * etc.) or Resource Group ids (can be obtained from `GET /rest/v{3-}/resourceGroups`) with
     * access rights for this Users Group.
     */
    std::optional<std::map<QnUuid, AccessRights>> resourceAccessRights;

    /**%apidoc[readonly] Whether this Role comes with the System. */
    bool isPredefined = false;

    bool operator==(const UserGroupModel& other) const = default;

    using DbReadTypes = std::tuple<UserGroupData, AccessRightsData>;
    using DbUpdateTypes = std::tuple<UserGroupData, std::optional<AccessRightsData>>;
    using DbListTypes = std::tuple<UserGroupDataList, AccessRightsDataList>;

    QnUuid getId() const { return id; }
    void setId(QnUuid id_) { id = std::move(id_); }

    static_assert(isCreateModelV<UserGroupModel>);
    static_assert(isUpdateModelV<UserGroupModel>);

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
    (isPredefined) \
    (externalId)
QN_FUSION_DECLARE_FUNCTIONS(UserGroupModel, (csv_record)(json)(ubjson)(xml), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(UserGroupModel, UserGroupModel_Fields)

} // namespace nx::vms::api
