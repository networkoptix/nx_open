#include "detach_rest_handler.h"

#include <cdb/connection.h>
#include <nx/network/http/httptypes.h>
#include <nx/utils/log/log.h>

#include <api/global_settings.h>
#include <api/model/cloud_credentials_data.h>
#include <api/model/detach_from_cloud_data.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>
#include <rest/server/rest_connection_processor.h>
#include <rest/helpers/permissions_helper.h>
#include <utils/common/sync_call.h>

#include <media_server/serverutil.h>
#include <cloud/cloud_connection_manager.h>
#include <utils/common/app_info.h>
#include <api/model/detach_from_cloud_reply.h>

namespace {

static const QString kDefaultAdminPassword = "admin";

} // namespace

QnDetachFromCloudRestHandler::QnDetachFromCloudRestHandler(
    CloudConnectionManager* const cloudConnectionManager)
    :
    QnJsonRestHandler(),
    m_cloudConnectionManager(cloudConnectionManager)
{
}

int QnDetachFromCloudRestHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParams& params,
    QnJsonRestResult &result,
    const QnRestConnectionProcessor* owner)
{
    return execute(std::move(DetachFromCloudData(params)), owner->accessRights(), result);
}

int QnDetachFromCloudRestHandler::executePost(
    const QString& /*path*/,
    const QnRequestParams& /*params*/,
    const QByteArray& body,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    DetachFromCloudData passwordData = QJson::deserialized<DetachFromCloudData>(body);
    return execute(std::move(passwordData), owner->accessRights(), result);
}

int QnDetachFromCloudRestHandler::execute(
    DetachFromCloudData data, const Qn::UserAccessData& accessRights, QnJsonRestResult& result)
{
    using namespace nx::cdb;

    if (QnPermissionsHelper::isSafeMode())
        return QnPermissionsHelper::safeModeError(result);
    if (!QnPermissionsHelper::hasOwnerPermissions(accessRights))
        return QnPermissionsHelper::notOwnerError(result);

    QString errStr;
    if (!validatePasswordData(data, &errStr))
    {
        result.setError(QnJsonRestResult::CantProcessRequest, errStr);
        result.setReply(DetachFromCloudReply(
            DetachFromCloudReply::ResultCode::invalidPasswordData));
        return nx_http::StatusCode::ok;
    }

    // first of all, enable admin user and changing its password
    //      so that there is always a way to connect to the system
    if (!updateUserCredentials(data, QnOptionalBool(true), qnResPool->getAdministrator(), &errStr))
    {
        result.setError(QnJsonRestResult::CantProcessRequest, errStr);
        result.setReply(DetachFromCloudReply(
            DetachFromCloudReply::ResultCode::cannotUpdateUserCredentials));
        return nx_http::StatusCode::ok;
    }

    // Second, updating data in cloud.
    api::ResultCode cdbResultCode = api::ResultCode::ok;
    auto systemId = qnGlobalSettings->cloudSystemID();
    auto authKey = qnGlobalSettings->cloudAuthKey();
    auto cloudConnection = m_cloudConnectionManager->getCloudConnection(systemId, authKey);
    std::tie(cdbResultCode) = makeSyncCall<api::ResultCode>(
        std::bind(
            &api::SystemManager::unbindSystem,
            cloudConnection->systemManager(),
            systemId.toStdString(),
            std::placeholders::_1));
    if (cdbResultCode != api::ResultCode::ok)
    {
        // TODO: #ak: Rollback "admin" user modification?

        NX_LOGX(lm("Received error response from %1: %2").arg(QnAppInfo::cloudName())
            .arg(api::toString(cdbResultCode)), cl_logWARNING);
        result.setError(
            QnJsonRestResult::CantProcessRequest,
            lit("Could not connect to %1: %2").arg(QnAppInfo::cloudName())
                .arg(QString::fromStdString(api::toString(cdbResultCode))));
        result.setReply(DetachFromCloudReply(
            DetachFromCloudReply::ResultCode::errorFromCloudServer,
            static_cast<int>(cdbResultCode)));
        return nx_http::StatusCode::ok;
    }

    if (!m_cloudConnectionManager->cleanUpCloudDataInLocalDb())
    {
        NX_LOGX(lit("Error resetting cloud credentials in local DB"), cl_logWARNING);
        result.setError(
            QnJsonRestResult::CantProcessRequest,
            lit("Failed to save cloud credentials to local DB"));
        result.setReply(DetachFromCloudReply(
            DetachFromCloudReply::ResultCode::cannotCleanUpCloudDataInLocalDb));
        return nx_http::StatusCode::internalServerError;
    }

    return nx_http::StatusCode::ok;
}
