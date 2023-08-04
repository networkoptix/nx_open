// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <optional>
#include <tuple>
#include <vector>

#include <nx/fusion/model_functions_fwd.h>

#include "global_permission_deprecated.h"
#include "resource_data.h"
#include "type_traits.h"
#include "user_data_ex.h"
#include "user_external_id.h"

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

    /**%apidoc[opt] Disabled users can not login. */
    bool isEnabled = true;

    /**%apidoc[opt]
     * Whether HTTP digest authorization can be used for this user. Requires password to enable.
     * Always `false` for Cloud users because digest support is controlled by the Cloud.
     */
    bool isHttpDigestEnabled = false;

    /**%apidoc The password for authorization. It's used only for `local` users. */
    std::optional<QString> password;

    /**%apidoc[proprietary] External identification data (currently used for LDAP only). */
    std::optional<UserExternalIdModel> externalId;

    /**%apidoc[readonly] */
    UserAttributes attributes{};

    // The next fields are used by PATCH functionality to preserve the existing DB data.
    std::optional<QnLatin1Array> digest; /**<%apidoc[unused] */
    std::optional<QnLatin1Array> hash; /**<%apidoc[unused] */
    std::optional<QnLatin1Array> cryptSha512Hash; /**<%apidoc[unused] */

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
    (isEnabled) \
    (isHttpDigestEnabled) \
    (externalId) \
    (attributes) \
    (password) \
    (digest) \
    (hash) \
    (cryptSha512Hash)
QN_FUSION_DECLARE_FUNCTIONS(UserModelBase, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(UserModelBase, UserModelBase_Fields)

// -------------------------------------------------------------------------------------------------

/**%apidoc User information object for REST v1 and v2
 * %param [unused] externalId
 * %param [unused] attributes
 */
struct NX_VMS_API UserModelV1: public UserModelBase
{
    /**%apidoc[proprietary] Intended for internal use; keep the value when saving a previously
     * received object, use false when creating a new one.
     */
    bool isOwner = false;

    /**%apidoc[opt] */
    GlobalPermissionsDeprecated permissions = GlobalPermissionDeprecated::none;

    /**%apidoc[opt] User Role id, can be obtained from `GET /rest/v{1-}/userRoles`. */
    QnUuid userRoleId;

    /**%apidoc
     * List of accessible Resource ids for the User. The access rights are calculated from the
     * `permissions` value when the User accesses a Resource.
     */
    std::optional<std::vector<QnUuid>> accessibleResources;

    bool operator==(const UserModelV1& other) const = default;

    using DbReadTypes = std::tuple<UserData>;
    using DbUpdateTypes = std::tuple<UserDataEx>;
    using DbListTypes = std::tuple<UserDataList>;

    DbUpdateTypes toDbTypes() &&;
    static std::vector<UserModelV1> fromDbTypes(DbListTypes data);

    static_assert(isCreateModelV<UserModelV1>);
    static_assert(isUpdateModelV<UserModelV1>);
};
#define UserModelV1_Fields UserModelBase_Fields(isOwner)(permissions)(userRoleId)(accessibleResources)
QN_FUSION_DECLARE_FUNCTIONS(UserModelV1, (json), NX_VMS_API)

// -------------------------------------------------------------------------------------------------

/**%apidoc[opt]
 * A token to authenticate temporary users.
 */
struct NX_VMS_API TemporaryToken
{
    /**%apidoc[opt] Temporary user validity period start timestamp from Epoch in seconds.
     * If it's not set (the value is less than zero), the current moment is implied.
     * %example 1689273703
     */
    std::chrono::seconds startS{-1};

    /**%apidoc Temporary user validity period end timestamp from Epoch in seconds.
     * %example 1689273704
     */
    std::chrono::seconds endS{-1};

    /**%apidoc[opt]
     * Temporary user validity expiration period after the first login.
     * The user becomes invalid after this period is over or if the `endS` has been reached
     * whichever happens first.
     * Value less than zero is treated as 'not set'.
     * %example 10000
     */
    std::chrono::seconds expiresAfterLoginS{-1};

    /**%apidoc[readonly]
     * Authentication token. It is filled by the Server in the response. This token may be
     * further used in the `rest/v{3-}/login/temporaryToken` request.
     */
    std::string token;

    void generateToken();
    bool isValid() const;
    static std::string prefix();
    bool operator==(const TemporaryToken& other) const = default;
};
#define TemporaryToken_Fields (startS)(endS)(expiresAfterLoginS)(token)
QN_FUSION_DECLARE_FUNCTIONS(TemporaryToken, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(TemporaryToken, TemporaryToken_Fields)

using TemporaryTokenList = std::vector<TemporaryToken>;

/**%apidoc User information object */
struct NX_VMS_API UserModelV3: public UserModelBase
{
    /**%apidoc[opt] User group id, can be obtained from `GET /rest/v{3-}/userGroups`. */
    std::vector<QnUuid> groupIds;

    /**%apidoc[opt] */
    GlobalPermissions permissions = GlobalPermission::none;

    /**%apidoc[opt]
     * Access rights per Resource (can be obtained from `/rest/v{3-}/devices`,
     * `/rest/v{3-}/servers`, etc.) or Resource Group (can be obtained from
     * `/rest/v{3-}/resourceGroups`).
     */
    std::map<QnUuid, AccessRights> resourceAccessRights;

    /**%apidoc
     * This token should be used only when the user type is `temporaryLocal`.
     */
    std::optional<TemporaryToken> temporaryToken;

    bool operator==(const UserModelV3& other) const = default;

    using DbReadTypes = std::tuple<UserData>;
    using DbUpdateTypes = std::tuple<UserDataEx>;
    using DbListTypes = std::tuple<UserDataList>;

    DbUpdateTypes toDbTypes() &&;
    static std::vector<UserModelV3> fromDbTypes(DbListTypes data);

    static_assert(isCreateModelV<UserModelV3>);
    static_assert(isUpdateModelV<UserModelV3>);
};
#define UserModelV3_Fields UserModelBase_Fields(groupIds)(permissions)(resourceAccessRights) \
    (temporaryToken)

QN_FUSION_DECLARE_FUNCTIONS(UserModelV3, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(UserModelV3, UserModelV3_Fields)

using UserModel = UserModelV3;

} // namespace nx::vms::api
