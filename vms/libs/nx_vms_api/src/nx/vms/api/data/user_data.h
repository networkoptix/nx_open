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

NX_REFLECTION_ENUM_CLASS(UserType,
    /**%apidoc This User is managed by VMS. */
    local = 0,

    /**%apidoc This User is managed by VMS and has a time limited access by the session token. */
    temporaryLocal = 3,

    /**%apidoc This User is imported from LDAP Server. */
    ldap = 1,

    /**%apidoc This is a Cloud User with access to this VMS System. */
    cloud = 2
)

NX_REFLECTION_ENUM_CLASS(UserAttribute,
    /**%apidoc This User can not be managed from VMS side. */
    readonly = 1 << 0,

    /**%apidoc This User should not be shown in user management by default. */
    hidden = 1 << 1
)

Q_DECLARE_FLAGS(UserAttributes, UserAttribute)
Q_DECLARE_OPERATORS_FOR_FLAGS(UserAttributes)

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

    UserData(const UserData& other) = default;
    UserData(UserData&& other) = default;
    UserData& operator=(const UserData& other) = default;
    UserData& operator=(UserData&& other) = default;
    bool operator==(const UserData& other) const = default;

    /** See fillId() in IdData. */
    void fillId();

    static const QString kResourceTypeName;
    static const QnUuid kResourceTypeId;

    static constexpr const char* kCloudPasswordStub = "password_is_in_cloud";
    static constexpr const char* kHttpIsDisabledStub = "http_is_disabled";

    /**%apidoc[opt] Type of the user. */
    UserType type = UserType::local;

    /**%apidoc[opt] Whether the user is enabled.*/
    bool isEnabled = true;

    /**%apidoc[opt] Full name of the user.*/
    QString fullName;

    /**%apidoc[opt] User's email.*/
    QString email;

    /**%apidoc[opt] Own user global permissions. */
    GlobalPermissions permissions = GlobalPermission::none;

    /**%apidoc[opt] List of groups to inherit permissions. */
    std::vector<QnUuid> groupIds;

    /**%apidoc[opt] Access rights per Resource or Resource Group. */
    std::map<QnUuid, AccessRights> resourceAccessRights;

    /** Checks if this user is a member of the Administrators group. */
    bool isAdministrator() const;

    /**%apidoc[proprietary] External identification data (currently used for LDAP only). */
    UserExternalId externalId;

    /**%apidoc[readonly] */
    UserAttributes attributes{};

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
};
#define UserData_Fields \
    ResourceData_Fields \
    (type) \
    (isEnabled) \
    (fullName) \
    (email) \
    (permissions) \
    (resourceAccessRights) \
    (groupIds) \
    (externalId) \
    (attributes) \
    (digest) \
    (hash) \
    (cryptSha512Hash)
NX_VMS_API_DECLARE_STRUCT_AND_LIST(UserData)
NX_REFLECTION_INSTRUMENT(UserData, UserData_Fields)

} // namespace nx::vms::api
