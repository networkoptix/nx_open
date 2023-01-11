// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>
#include <tuple>
#include <type_traits>

#include <nx/fusion/model_functions_fwd.h>

#include "access_rights_data.h"
#include "resource_data.h"
#include "type_traits.h"
#include "user_data_ex.h"

namespace nx::vms::api {

/**%apidoc
 * User information object.
 */
struct NX_VMS_API UserModel
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

    /**%apidoc[opt] User role id, can be obtained form /rest/v1/userRoles. */
    QnUuid userRoleId;

    /**%apidoc List of accessible resource ids for the user. */
    std::optional<std::vector<QnUuid>> accessibleResources;

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

    using DbReadTypes = std::tuple<UserData, AccessRightsData>;
    using DbUpdateTypes = std::tuple<UserDataEx, std::optional<AccessRightsData>>;
    using DbListTypes = std::tuple<UserDataList, AccessRightsDataList>;

    bool operator==(const UserModel& other) const = default;

    QnUuid getId() const { return id; }
    void setId(QnUuid id_) { id = std::move(id_); }

    static_assert(isCreateModelV<UserModel>);
    static_assert(isUpdateModelV<UserModel>);

    DbUpdateTypes toDbTypes() &&;
    static std::vector<UserModel> fromDbTypes(DbListTypes data);
};
#define UserModel_Fields \
    (id)(name)(type)(fullName)(email) \
    (isOwner)(permissions)(userRoleId)(accessibleResources) \
    (isEnabled)(isHttpDigestEnabled)(password) \
    (digest)(hash)(cryptSha512Hash)(realm)

QN_FUSION_DECLARE_FUNCTIONS(UserModel, (csv_record)(json)(ubjson)(xml), NX_VMS_API)

} // namespace nx::vms::api
