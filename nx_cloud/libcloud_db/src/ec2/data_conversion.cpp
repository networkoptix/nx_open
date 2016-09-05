#include "data_conversion.h"

#include <cdb/system_data.h>

#include <nx_ec/data/api_user_data.h>
#include <nx/fusion/serialization/lexical.h>

#include <utils/common/app_info.h>

namespace nx {
namespace cdb {
namespace ec2 {

api::SystemAccessRole permissionsToAccessRole(Qn::GlobalPermissions permissions)
{
    switch (permissions)
    {
        case Qn::GlobalLiveViewerPermissionSet:
            return api::SystemAccessRole::liveViewer;
        case Qn::GlobalViewerPermissionSet:
            return api::SystemAccessRole::viewer;
        case Qn::GlobalAdvancedViewerPermissionSet:
            return api::SystemAccessRole::advancedViewer;
        case Qn::GlobalAdminPermissionSet:
            return api::SystemAccessRole::cloudAdmin;
        default:
            return api::SystemAccessRole::custom;
    }
}

void accessRoleToPermissions(
    api::SystemAccessRole accessRole,
    Qn::GlobalPermissions* const permissions,
    bool* const isAdmin)
{
    *isAdmin = false;
    switch (accessRole)
    {
        case api::SystemAccessRole::owner:
            *permissions = Qn::GlobalAdminPermissionSet;
            *isAdmin = true;   //aka "owner"
            break;
        case api::SystemAccessRole::liveViewer:
            *permissions = Qn::GlobalLiveViewerPermissionSet;
            break;
        case api::SystemAccessRole::viewer:
            *permissions = Qn::GlobalViewerPermissionSet;
            break;
        case api::SystemAccessRole::advancedViewer:
            *permissions = Qn::GlobalAdvancedViewerPermissionSet;
            break;
        case api::SystemAccessRole::cloudAdmin:
            *permissions = Qn::GlobalAdminPermissionSet;
            break;
        case api::SystemAccessRole::custom:
        default:
            *permissions = Qn::NoGlobalPermissions;
            break;
    }
}

void convert(const api::SystemSharing& from, ::ec2::ApiUserData* const to)
{
    to->id = QnUuid::fromStringSafe(from.vmsUserId);
    to->email = QString::fromStdString(from.accountEmail);
    to->name = to->email;
    to->permissions = 
        QnLexical::deserialized<Qn::GlobalPermissions>(
            QString::fromStdString(from.customPermissions));
    to->groupId = QnUuid::fromStringSafe(from.groupID);
    to->isEnabled = from.isEnabled;
    to->realm = QnAppInfo::realm();
    to->hash = "password_is_in_cloud";
    to->digest = "password_is_in_cloud";
    accessRoleToPermissions(from.accessRole, &to->permissions, &to->isAdmin);
}

void convert(const ::ec2::ApiUserData& from, api::SystemSharing* const to)
{
    to->accountEmail = from.email.toStdString();
    to->customPermissions = QnLexical::serialized(from.permissions).toStdString();
    to->groupID = from.groupId.toSimpleString().toStdString();
    to->isEnabled = from.isEnabled;
    to->accessRole =
        from.isAdmin
        ? api::SystemAccessRole::owner
        : permissionsToAccessRole(from.permissions);
    to->vmsUserId = from.id.toSimpleString().toStdString();
}

void convert(const api::SystemSharing& from, ::ec2::ApiIdData* const to)
{
    to->id = QnUuid(from.vmsUserId);
}

} // namespace ec2
} // namespace cdb
} // namespace nx
