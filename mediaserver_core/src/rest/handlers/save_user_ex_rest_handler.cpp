#include "save_user_ex_rest_handler.h"
#include <nx/vms/api/data/user_data_ex.h>
#include <nx/vms/api/data/user_data.h>
#include <api/model/password_data.h>
#include <common/common_module.h>
#include <media_server/media_server_module.h>
#include <nx_ec/ec_api.h>
#include <nx_ec/managers/abstract_user_manager.h>
#include <rest/server/rest_connection_processor.h>
#include <core/resource_access/resource_access_manager.h>

QnSaveUserExRestHandler::QnSaveUserExRestHandler(QnMediaServerModule* serverModule):
    nx::mediaserver::ServerModuleAware(serverModule)
{}

int QnSaveUserExRestHandler::executePost(
    const QString& /*path*/,
    const QnRequestParams& /*params*/,
    const QByteArray& body,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    using namespace nx::vms::api;

    if (!owner->resourceAccessManager()->hasGlobalPermission(
            owner->accessRights(),
            GlobalPermission::adminPermissions))
    {
        result.setError(QnJsonRestResult::Forbidden);
        return nx::network::http::StatusCode::ok;
    }

    UserDataEx userDataEx;
    if (!QJson::deserialize<UserDataEx>(body, &userDataEx))
    {
        result.setError(QnJsonRestResult::InvalidParameter, lit("Invalid Json object provided"));
        return nx::network::http::StatusCode::ok;
    }

    UserData userData;
    userData.id = userDataEx.id;
    userData.name = userDataEx.name;
    userData.permissions = userDataEx.permissions;
    userData.email = userDataEx.email;
    userData.isCloud = userDataEx.isCloud;
    userData.isLdap = userDataEx.isLdap;
    userData.isEnabled = userDataEx.isEnabled;
    userData.userRoleId = userDataEx.userRoleId;
    userData.fullName = userDataEx.fullName;

    const auto hashes = PasswordData::calculateHashes(
        userData.name,
        userDataEx.name,
        userData.isLdap);

    userData.realm = hashes.realm;
    userData.hash = hashes.passwordHash;
    userData.digest = hashes.passwordDigest;
    userData.cryptSha512Hash = hashes.cryptSha512Hash;

    auto ec2Connection = serverModule()->commonModule()->ec2Connection();
    if (!ec2Connection)
    {
        result.setError(QnJsonRestResult::CantProcessRequest, lit("No ec2 connection"));
        return nx::network::http::StatusCode::ok;
    }

    auto userManager = ec2Connection->getUserManager(owner->accessRights());
    if (userManager->saveSync(userData) != ec2::ErrorCode::ok)
    {
        result.setError(QnJsonRestResult::CantProcessRequest, lit("Failed to save user"));
        return nx::network::http::StatusCode::ok;
    }

    return nx::network::http::StatusCode::ok;
}

