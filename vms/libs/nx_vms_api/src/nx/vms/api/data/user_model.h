// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <optional>
#include <tuple>
#include <vector>

#include <nx/fusion/model_functions_fwd.h>

#include "access_rights_data.h"
#include "resource_data.h"
#include "type_traits.h"
#include "user_data_ex.h"

namespace nx::vms::api {

/**%apidoc
 * User information object.
 */
struct NX_VMS_API UserModelBase
{
    QnUuid id;

    /**%apidoc
     * %example admin
     */
    QString name;

    /**%apidoc[opt] For cloud users this should be equal name. */
    QString email;

    /**%apidoc[opt] Only local users are supposed to be created by API. */
    UserType type = UserType::local;

    /**%apidoc[opt] Full name. */
    QString fullName;

   /**%apidoc[proprietary] Intended for internal use; keep the value when saving a previously
     * received object, use false when creating a new one.
     */
    bool isOwner = false;

    /**%apidoc[opt] */
    GlobalPermissions permissions = GlobalPermission::none;

    /**%apidoc[opt] Disabled users can not login. */
    bool isEnabled = true;

    /**%apidoc[opt]
     * Whether HTTP digest authorization can be used for this user. Requires password to enable.
     * Always `false` for Cloud users because digest support is controlled by the Cloud.
     */
    bool isHttpDigestEnabled = false;

    /**%apidoc The password for authorization. */
    std::optional<QString> password;

    // The next fields are used by PATCH functionality to preserve the existing DB data.
    std::optional<QnLatin1Array> digest; /**<%apidoc[unused] */
    std::optional<QnLatin1Array> hash; /**<%apidoc[unused] */
    std::optional<QnLatin1Array> cryptSha512Hash; /**<%apidoc[unused] */
    std::optional<QString> realm; /**<%apidoc[unused] */

    bool operator==(const UserModelBase& other) const = default;

    QnUuid getId() const { return id; }
    void setId(QnUuid id_) { id = std::move(id_); }

    UserDataEx toUserData() &&;
    static UserModelBase fromUserData(UserData&& baseData);
};
#define UserModelBase_Fields \
    (id) \
    (name) \
    (type) \
    (fullName) \
    (email) \
    (isOwner) \
    (permissions) \
    (isEnabled) \
    (isHttpDigestEnabled) \
    (password) \
    (digest) \
    (hash) \
    (cryptSha512Hash) \
    (realm)
QN_FUSION_DECLARE_FUNCTIONS(UserModelBase, (csv_record)(json)(ubjson)(xml), NX_VMS_API)

// -------------------------------------------------------------------------------------------------

struct NX_VMS_API UserModelV1: public UserModelBase
{
    /**%apidoc[opt] User Role id, can be obtained from `GET /rest/v{1-}/userRoles`. */
    QnUuid userRoleId;

    /**%apidoc
     * List of accessible Resource ids for the User. The access rights are calculated from the
     * `permissions` value when the User accesses a Resource.
     */
    std::optional<std::vector<QnUuid>> accessibleResources;

    /**%apidoc[unused]
     * %// This field is invisible but required for PATCH.
     */
    std::optional<QString> externalId;

    bool operator==(const UserModelV1& other) const = default;

    using DbReadTypes = std::tuple<UserData, AccessRightsData>;
    using DbUpdateTypes = std::tuple<UserDataEx, std::optional<AccessRightsData>>;
    using DbListTypes = std::tuple<UserDataList, AccessRightsDataList>;

    DbUpdateTypes toDbTypes() &&;
    static std::vector<UserModelV1> fromDbTypes(DbListTypes data);

    static_assert(isCreateModelV<UserModelV1>);
    static_assert(isUpdateModelV<UserModelV1>);
};
#define UserModelV1_Fields UserModelBase_Fields(userRoleId)(accessibleResources)(externalId)
QN_FUSION_DECLARE_FUNCTIONS(UserModelV1, (csv_record)(json)(ubjson)(xml), NX_VMS_API)

// -------------------------------------------------------------------------------------------------

/**
 * This is for the new expiremental API which was supposed to be introduced in /rest/v2 but is
 * going to be delayed for the future API versions.
 */
struct NX_VMS_API UserModelV3: public UserModelBase
{
    /**%apidoc[opt] User group id, can be obtained from `GET /rest/v{3-}/userGroups`. */
    std::vector<QnUuid> userGroupIds;

    /**%apidoc[readonly]
     * An external id, for example, a Distinguished Name (DN) if this User is imported from LDAP.
     */
    std::optional<QString> externalId;

    /**%apidoc Resource ids with access rights for the User. */
    std::optional<std::map<QnUuid, AccessRights>> resourceAccessRights;

    bool operator==(const UserModelV3& other) const = default;

    using DbReadTypes = std::tuple<UserData, AccessRightsData>;
    using DbUpdateTypes = std::tuple<UserDataEx, std::optional<AccessRightsData>>;
    using DbListTypes = std::tuple<UserDataList, AccessRightsDataList>;

    DbUpdateTypes toDbTypes() &&;
    static std::vector<UserModelV3> fromDbTypes(DbListTypes data);

    static_assert(isCreateModelV<UserModelV3>);
    static_assert(isUpdateModelV<UserModelV3>);
};
#define UserModelV3_Fields UserModelBase_Fields(userGroupIds)(externalId)(resourceAccessRights)

QN_FUSION_DECLARE_FUNCTIONS(UserModelV3, (csv_record)(json)(ubjson)(xml), NX_VMS_API)

using UserModel = UserModelV3;

} // namespace nx::vms::api
