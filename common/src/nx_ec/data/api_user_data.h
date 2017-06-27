#pragma once

#include <nx/network/app_info.h>

#include "api_globals.h"
#include "api_resource_data.h"
#include <core/resource/resource_type.h>
#include <utils/common/app_info.h>

namespace ec2 {

struct ApiUserData: ApiResourceData
{
    static constexpr const char* kCloudPasswordStub = "password_is_in_cloud";

    ApiUserData():
        isAdmin(false),
        permissions(Qn::NoGlobalPermissions),
        realm(nx::network::AppInfo::realm()),
        isLdap(false),
        isEnabled(true),
        isCloud(false),
        fullName()
    {
        typeId = QnResourceTypePool::kUserTypeUuid;
    }

    /**
     * See fillId() in ApiIdData.
     */
    void fillId()
    {
         // ATTENTION: This logic is similar to QnUserResource::fillId().
         if (isCloud)
         {
             if (!email.isEmpty())
                 id = guidFromArbitraryData(email);
             else
                 id = QnUuid();
         }
         else
         {
             id = QnUuid::createUuid();
         }
    }

    /**
     * Actually, this flag should be named isOwner, but we must keep it to maintain mobile client
     * ompatibility.
     */
    bool isAdmin;

    /** Global user permissions. */
    Qn::GlobalPermissions permissions;

    QnUuid userRoleId;

    QString email;
    QnLatin1Array digest;
    QnLatin1Array hash;
    QnLatin1Array cryptSha512Hash; /**< Hash suitable to be used in /etc/shadow file. */
    QString realm;
    bool isLdap;
    bool isEnabled;

    /** Whether the user is created from the Cloud. */
    bool isCloud;

    /** Full user name. */
    QString fullName;

    bool operator==(const ApiUserData& rhs) const;
};
#define ApiUserData_Fields ApiResourceData_Fields \
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

} // namespace ec2
