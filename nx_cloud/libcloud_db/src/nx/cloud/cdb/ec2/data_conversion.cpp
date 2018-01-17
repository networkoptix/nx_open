#include "data_conversion.h"

#include <nx_ec/data/api_user_data.h>
#include <nx/fusion/serialization/lexical.h>

namespace nx {
namespace cdb {
namespace ec2 {

namespace {
static const QnUuid kUserResourceTypeGuid("{774e6ecd-ffc6-ae88-0165-8f4a6d0eafa7}");
} // namespace

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
    to->typeId = kUserResourceTypeGuid;
    to->email = QString::fromStdString(from.accountEmail);
    to->name = to->email;
    to->permissions =
        QnLexical::deserialized<Qn::GlobalPermissions>(
            QString::fromStdString(from.customPermissions));
    to->userRoleId = QnUuid::fromStringSafe(from.userRoleId);
    to->isEnabled = from.isEnabled;
    to->realm = nx::network::AppInfo::realm();
    to->hash = "password_is_in_cloud";
    to->digest = "password_is_in_cloud";
    to->isCloud = true;
    accessRoleToPermissions(from.accessRole, &to->permissions, &to->isAdmin);
}

void convert(const ::ec2::ApiUserData& from, api::SystemSharing* const to)
{
    to->accountEmail = from.email.toStdString();
    to->customPermissions = QnLexical::serialized(from.permissions).toStdString();
    to->userRoleId = from.userRoleId.toSimpleString().toStdString();
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
