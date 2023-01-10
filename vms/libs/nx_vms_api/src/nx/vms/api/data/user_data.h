// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include <nx/reflect/enum_instrument.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/latin1_array.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/types/access_rights_types.h>

#include "resource_data.h"
#include "user_external_id.h"

namespace nx::vms::api {

/**%apidoc User information object.
 * %param[readonly] id Internal user identifier. This identifier can be used as {id} path parameter in
 *     requests related to user.
 * %param[proprietary] parentId Should be empty.
 * %param name User name.
 * %param[proprietary] url Should be empty.
 * %param[proprietary] typeId Should have fixed value.
 *     %value {774e6ecd-ffc6-ae88-0165-8f4a6d0eafa7}
 */
struct NX_VMS_API UserData: ResourceData
{
    UserData(): ResourceData(kResourceTypeId) {}

    bool operator==(const UserData& other) const = default;

    /** See fillId() in IdData. */
    void fillId();

    static const QString kResourceTypeName;
    static const QnUuid kResourceTypeId;

    static constexpr const char* kCloudPasswordStub = "password_is_in_cloud";
    static constexpr const char* kHttpIsDisabledStub = "http_is_disabled";

    /**
     * Actually, this flag should be named isOwner, but we must keep it to maintain mobile client
     * compatibility.
     */
    /**%apidoc[proprietary] Intended for internal use; keep the value when saving a previously
     * received object, use false when creating a new one.
     */
     bool isAdmin = false;

    GlobalPermissions permissions = GlobalPermission::none;

    /**%apidoc[opt]
     * User role unique id.
     * %deprecated Use `userRoleIds` instead.
     */
    QnUuid userRoleId;

    /**%apidoc[opt] List of roles to inherit permissions. */
    std::vector<QnUuid> userRoleIds; //< TODO: Rename to userGroupIds on general renaming.

    /**%apidoc[opt] User's email.*/
    QString email;

    /**%apidoc[proprietary] HA1 digest hash from user password, as per RFC 2069. When modifying an
     * existing user, supply empty string. When creating a new user, calculate the value based on
     * UTF-8 password as follows:
     * <code>digest = md5_hex(user_name + ":" + realm + ":" + password);</code>
     */
    QnLatin1Array digest;

    /**%apidoc[proprietary] User's password hash. When modifying an existing user, supply empty
     * string. When creating a new user, see QnUserHash for detailed options.
     */
    QnLatin1Array hash;

    /**%apidoc[proprietary] Cryptography key hash. Supply empty string when creating, keep the
     * value when modifying.
     */
    QnLatin1Array cryptSha512Hash; /**< Hash suitable to be used in /etc/shadow file.*/

    /**%apidoc[proprietary] HTTP authorization realm as defined in RFC 2617, can be obtained via
     * /api/getTime.
     */
    QString realm;

    /**%apidoc[opt] Whether the user was imported from LDAP.*/
    bool isLdap = false;

    /**%apidoc[opt] Whether the user is enabled.*/
    bool isEnabled = true;

    /**%apidoc[opt] Whether the user is a cloud user, as opposed to a local one.*/
    bool isCloud = false;

    /**%apidoc[opt] Full name of the user.*/
    QString fullName;

    /**%apidoc[readonly] External identification data (currently used for LDAP only). */
    UserExternalId externalId;

    // TODO: Remove when /ec2/getUsers and /ec2/saveUser compatibility below 5.1 can be dropped.
    bool adaptFromDeprecatedApi();
    void cleanOnDeprecatedApiMerge(const QJsonValue& overrideValue);
    void adaptForDeprecatedApi();
};
#define UserData_Fields \
    ResourceData_Fields \
    (isAdmin) \
    (permissions) \
    (email) \
    (digest) \
    (hash) \
    (cryptSha512Hash) \
    (realm) \
    (isLdap) \
    (isEnabled) \
    (userRoleId)  \
    (isCloud)  \
    (fullName) \
    (userRoleIds) \
    (externalId)
NX_VMS_API_DECLARE_STRUCT_AND_LIST(UserData)
NX_REFLECTION_INSTRUMENT(UserData, UserData_Fields)

// TODO: UserData should be refactored to use this enum, when we can break API compatibility.
NX_REFLECTION_ENUM_CLASS(UserType,
    local,
    ldap,
    cloud
)

NX_VMS_API UserType type(const UserData& user);
NX_VMS_API void setType(UserData* user, UserType type);

} // namespace nx::vms::api

