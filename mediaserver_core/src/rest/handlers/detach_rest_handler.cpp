#include "detach_rest_handler.h"

#include <api/global_settings.h>

#include <common/common_module.h>

#include <nx/network/http/httptypes.h>
#include "media_server/serverutil.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>
#include "rest/server/rest_connection_processor.h"
#include <rest/helpers/permissions_helper.h>
#include "save_cloud_system_credentials.h"
#include <api/model/cloud_credentials_data.h>
#include <api/model/detach_from_cloud_data.h>

namespace {
    static const QString kDefaultAdminPassword = "admin";
}

QnDetachFromCloudRestHandler::QnDetachFromCloudRestHandler(
    const CloudConnectionManager& cloudConnectionManager)
    :
    QnJsonRestHandler(),
    m_cloudConnectionManager(cloudConnectionManager)
{

}

int QnDetachFromCloudRestHandler::executeGet(
    const QString& path,
    const QnRequestParams& params,
    QnJsonRestResult &result,
    const QnRestConnectionProcessor* owner)
{
    Q_UNUSED(path);
    return execute(std::move(DetachFromCloudData(params)), owner->authUserId(), result);
}

int QnDetachFromCloudRestHandler::executePost(
    const QString &path,
    const QnRequestParams &params,
    const QByteArray &body,
    QnJsonRestResult &result,
    const QnRestConnectionProcessor* owner)
{
    Q_UNUSED(path);
    Q_UNUSED(params);

    DetachFromCloudData passwordData = QJson::deserialized<DetachFromCloudData>(body);
    return execute(std::move(passwordData), owner->authUserId(), result);
}

int QnDetachFromCloudRestHandler::execute(DetachFromCloudData data, const QnUuid &userId, QnJsonRestResult &result)
{
    if (QnPermissionsHelper::isSafeMode())
        return QnPermissionsHelper::safeModeError(result);
    if (!QnPermissionsHelper::hasOwnerPermissions(userId))
        return QnPermissionsHelper::notOwnerError(result);

    QString errStr;
    if (!validatePasswordData(data, &errStr))
    {
        result.setError(QnJsonRestResult::CantProcessRequest, errStr);
        return nx_http::StatusCode::ok;
    }

    // enable admin user and change its password
    if (!updateUserCredentials(data, QnOptionalBool(true), qnResPool->getAdministrator(), &errStr))
    {
        result.setError(QnJsonRestResult::CantProcessRequest, errStr);
        return nx_http::StatusCode::ok;
    }

    // disable cloud users
    auto usersToDisable = qnResPool->getResources<QnUserResource>().filtered(
        [](const QnUserResourcePtr& user)
        {
            return user->isEnabled() && user->isCloud();
        });
    for (const auto& user: usersToDisable)
    {
        if (!updateUserCredentials(PasswordData(), QnOptionalBool(false), user, &errStr))
        {
            result.setError(QnJsonRestResult::CantProcessRequest, errStr);
            return nx_http::StatusCode::ok;
        }
    }

    CloudCredentialsData cloudCredentials;
    cloudCredentials.reset = true;
    QnSaveCloudSystemCredentialsHandler subHandler(m_cloudConnectionManager);
    int httpResult = subHandler.execute(cloudCredentials, result);
    if (result.error != QnJsonRestResult::NoError)
        return nx_http::StatusCode::ok;

    qnCommon->updateModuleInformation();
    return nx_http::StatusCode::ok;
}
