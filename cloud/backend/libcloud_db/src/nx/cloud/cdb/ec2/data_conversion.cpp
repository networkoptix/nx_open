#include "data_conversion.h"

#include <nx_ec/data/api_fwd.h>

#include <nx/fusion/serialization/lexical.h>
#include <nx/network/app_info.h>
#include <nx/vms/api/data/id_data.h>
#include <nx/vms/api/data/user_data.h>

namespace nx::cloud::db {
namespace ec2 {

namespace {

static const QnUuid kUserResourceTypeGuid("{774e6ecd-ffc6-ae88-0165-8f4a6d0eafa7}");

} // namespace

api::SystemAccessRole permissionsToAccessRole(GlobalPermissions permissions)
{
    switch (permissions)
    {
        case (int) GlobalPermission::liveViewerPermissions:
            return api::SystemAccessRole::liveViewer;
        case (int) GlobalPermission::viewerPermissions:
            return api::SystemAccessRole::viewer;
        case (int) GlobalPermission::advancedViewerPermissions:
            return api::SystemAccessRole::advancedViewer;
        case (int) GlobalPermission::adminPermissions:
            return api::SystemAccessRole::cloudAdmin;
        default:
            return api::SystemAccessRole::custom;
    }
}

void accessRoleToPermissions(
    api::SystemAccessRole accessRole,
    GlobalPermissions* const permissions,
    bool* const isAdmin)
{
    *isAdmin = false;
    switch (accessRole)
    {
        case api::SystemAccessRole::owner:
            *permissions = GlobalPermission::adminPermissions;
            *isAdmin = true;   //aka "owner"
            break;
        case api::SystemAccessRole::liveViewer:
            *permissions = GlobalPermission::liveViewerPermissions;
            break;
        case api::SystemAccessRole::viewer:
            *permissions = GlobalPermission::viewerPermissions;
            break;
        case api::SystemAccessRole::advancedViewer:
            *permissions = GlobalPermission::advancedViewerPermissions;
            break;
        case api::SystemAccessRole::cloudAdmin:
            *permissions = GlobalPermission::adminPermissions;
            break;
        case api::SystemAccessRole::custom:
        default:
            *permissions = {};
            break;
    }
}

void convert(const api::SystemSharing& from, vms::api::UserData* const to)
{
    to->id = QnUuid::fromStringSafe(from.vmsUserId);
    to->typeId = kUserResourceTypeGuid;
    to->email = QString::fromStdString(from.accountEmail);
    to->name = to->email;
    to->permissions =
        QnLexical::deserialized<GlobalPermissions>(
            QString::fromStdString(from.customPermissions));
    to->userRoleId = QnUuid::fromStringSafe(from.userRoleId);
    to->isEnabled = from.isEnabled;
    to->realm = nx::network::AppInfo::realm();
    to->hash = vms::api::UserData::kCloudPasswordStub;
    to->digest = vms::api::UserData::kCloudPasswordStub;
    to->isCloud = true;
    accessRoleToPermissions(from.accessRole, &to->permissions, &to->isAdmin);
}

void convert(const vms::api::UserData& from, api::SystemSharing* const to)
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

void convert(const api::SystemSharing& from, nx::vms::api::IdData* const to)
{
    to->id = QnUuid(from.vmsUserId);
}

} // namespace ec2
} // namespace nx::cloud::db
