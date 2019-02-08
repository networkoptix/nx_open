#pragma once

#include "resource_data.h"

#include <QtCore/QString>

#include <nx/utils/latin1_array.h>
#include <nx/vms/api/types/access_rights_types.h>

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API UserData: ResourceData
{
    UserData(): ResourceData(kResourceTypeId) {}

    /** See fillId() in IdData. */
    void fillId();

    static const QString kResourceTypeName;
    static const QnUuid kResourceTypeId;

    static constexpr const char* kCloudPasswordStub = "password_is_in_cloud";

    /**
     * Actually, this flag should be named isOwner, but we must keep it to maintain mobile client
     * compatibility.
     */
    bool isAdmin = false;

    /** Global user permissions. */
    GlobalPermissions permissions;

    QnUuid userRoleId;

    QString email;
    QnLatin1Array digest;
    QnLatin1Array hash;
    QnLatin1Array cryptSha512Hash; /**< Hash suitable to be used in /etc/shadow file. */
    QString realm;
    bool isLdap = false;
    bool isEnabled = true;

    /** Whether the user is created from the Cloud. */
    bool isCloud = false;

    /** Full user name. */
    QString fullName;
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
    (fullName)

} // namespace api
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::api::UserData)
Q_DECLARE_METATYPE(nx::vms::api::UserDataList)
