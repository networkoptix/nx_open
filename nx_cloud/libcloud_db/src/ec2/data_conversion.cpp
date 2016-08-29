/**********************************************************
* Aug 26, 2016
* a.kolesnikov
***********************************************************/

#include "data_conversion.h"

#include <cdb/system_data.h>

#include <nx_ec/data/api_user_data.h>
#include <nx/fusion/serialization/lexical.h>


namespace nx {
namespace cdb {
namespace ec2 {

namespace {
static api::SystemAccessRole permissionsToAccessRole(Qn::GlobalPermissions permissions)
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

static void accessRoleToPermissions(
    api::SystemAccessRole accessRole,
    ::ec2::ApiUserData* const userData)
{
    userData->isAdmin = false;
    switch (accessRole)
    {
        case api::SystemAccessRole::owner:
            userData->permissions = Qn::GlobalAdminPermissionSet;
            userData->isAdmin = true;   //aka "owner"
            break;
        case api::SystemAccessRole::liveViewer:
            userData->permissions = Qn::GlobalLiveViewerPermissionSet;
            break;
        case api::SystemAccessRole::viewer:
            userData->permissions = Qn::GlobalViewerPermissionSet;
            break;
        case api::SystemAccessRole::advancedViewer:
            userData->permissions = Qn::GlobalAdvancedViewerPermissionSet;
            break;
        case api::SystemAccessRole::cloudAdmin:
            userData->permissions = Qn::GlobalAdminPermissionSet;
            break;
        case api::SystemAccessRole::custom:
        default:
            userData->permissions = Qn::NoGlobalPermissions;
            break;
    }
}
}   // namespace

void convert(const api::SystemSharing& from, ::ec2::ApiUserData* const to)
{
    to->id = QnUuid::fromStringSafe(from.vmsUserId);
    to->email = QString::fromStdString(from.accountEmail);
    to->permissions = QnLexical::deserialized<Qn::GlobalPermissions>(QString::fromStdString(from.customPermissions));
    to->groupId = QnUuid::fromStringSafe(from.groupID);
    to->isEnabled = from.isEnabled;
    accessRoleToPermissions(from.accessRole, to);
}

void convert(const ::ec2::ApiUserData& from, api::SystemSharing* const to)
{
    to->accountEmail = from.email.toStdString();
    to->customPermissions = QnLexical::serialized(from.permissions).toStdString();
    to->groupID = from.groupId.toStdString();
    to->isEnabled = from.isEnabled;
    to->accessRole =
        from.isAdmin
        ? api::SystemAccessRole::owner
        : permissionsToAccessRole(from.permissions);
    to->vmsUserId = from.id.toStdString();
}

void convert(const api::SystemSharing& from, ::ec2::ApiIdData* const to)
{
    to->id = QnUuid(from.vmsUserId);
}

}   // namespace ec2
}   // namespace cdb
}   // namespace nx
