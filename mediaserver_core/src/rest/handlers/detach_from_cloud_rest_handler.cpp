#include "detach_from_cloud_rest_handler.h"

#include <nx/cloud/cdb/api/connection.h>
#include <nx/network/http/http_types.h>
#include <nx/utils/log/log.h>

#include <nx/vms/utils/vms_utils.h>

#include <api/global_settings.h>
#include <api/model/cloud_credentials_data.h>
#include <api/model/detach_from_cloud_data.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>
#include <rest/server/rest_connection_processor.h>
#include <rest/helpers/permissions_helper.h>
#include <network/system_helpers.h>
#include <nx/utils/sync_call.h>

#include <media_server/serverutil.h>
#include <nx/vms/cloud_integration/cloud_connection_manager.h>
#include <utils/common/app_info.h>
#include <api/model/detach_from_cloud_reply.h>

QnDetachFromCloudRestHandler::QnDetachFromCloudRestHandler(
    nx::vms::cloud_integration::CloudConnectionManager* const cloudConnectionManager)
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
    return execute(std::move(DetachFromCloudData(params)), owner, result);
}

int QnDetachFromCloudRestHandler::executePost(
    const QString& /*path*/,
    const QnRequestParams& /*params*/,
    const QByteArray& body,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    DetachFromCloudData passwordData = QJson::deserialized<DetachFromCloudData>(body);
    return execute(std::move(passwordData), owner, result);
}

int QnDetachFromCloudRestHandler::execute(
    DetachFromCloudData data, const QnRestConnectionProcessor* owner, QnJsonRestResult& result)
{
    using namespace nx::cdb;
    const Qn::UserAccessData& accessRights = owner->accessRights();

    NX_LOGX(lm("Detaching system from cloud. cloudSystemId %1")
        .arg(owner->globalSettings()->cloudSystemId()), cl_logDEBUG2);

    if (QnPermissionsHelper::isSafeMode(owner->commonModule()))
    {
        NX_LOGX(lm("Cannot detach from cloud while in safe mode. cloudSystemId %1")
            .arg(owner->globalSettings()->cloudSystemId()), cl_logDEBUG1);
        return QnPermissionsHelper::safeModeError(result);
    }
    if (!QnPermissionsHelper::hasOwnerPermissions(owner->resourcePool(), accessRights))
    {
        NX_LOGX(lm("Cannot detach from cloud. Owner permissions are required. cloudSystemId %1")
            .arg(owner->globalSettings()->cloudSystemId()), cl_logDEBUG1);
        return QnPermissionsHelper::notOwnerError(result);
    }

    QString errStr;
    if (!nx::vms::utils::validatePasswordData(data, &errStr))
    {
        NX_LOGX(lm("Cannot detach from cloud. Password check failed. cloudSystemId %1")
            .arg(owner->globalSettings()->cloudSystemId()), cl_logDEBUG1);
        result.setError(QnJsonRestResult::CantProcessRequest, errStr);
        result.setReply(DetachFromCloudReply(
            DetachFromCloudReply::ResultCode::invalidPasswordData));
        return nx_http::StatusCode::ok;
    }

    auto adminUser = owner->resourcePool()->getAdministrator();
    const bool shouldResetSystemToNewState = !adminUser->isEnabled() && !data.hasPassword();

    if (shouldResetSystemToNewState)
    {
        NX_VERBOSE(this, lm("Resetting system to the \"new\" state"));
        if (!nx::vms::utils::resetSystemToStateNew(owner->commonModule()))
        {
            NX_LOGX(lm("Cannot detach from cloud. Failed to reset system to state new. cloudSystemId %1")
                .arg(owner->globalSettings()->cloudSystemId()), cl_logDEBUG1);
            result.setError(QnJsonRestResult::CantProcessRequest, errStr);
            result.setReply(DetachFromCloudReply(
                DetachFromCloudReply::ResultCode::cannotUpdateUserCredentials));
            return nx_http::StatusCode::ok;
        }
    }
    else
    {
        NX_VERBOSE(this, lm("Enabling admin user"));

        // first of all, enable admin user and changing its password
        //      so that there is always a way to connect to the system
        if (!updateUserCredentials(
                owner->commonModule()->ec2Connection(),
                data,
                QnOptionalBool(true),
                adminUser,
                &errStr))
        {
            NX_LOGX(lm("Cannot detach from cloud. Failed to re-enable local admin. cloudSystemId %1")
                .arg(owner->globalSettings()->cloudSystemId()), cl_logDEBUG1);
            result.setError(QnJsonRestResult::CantProcessRequest, errStr);
            result.setReply(DetachFromCloudReply(
                DetachFromCloudReply::ResultCode::cannotUpdateUserCredentials));
            return nx_http::StatusCode::ok;
        }
    }

    // Second, updating data in cloud.
    api::ResultCode cdbResultCode = api::ResultCode::ok;
    auto systemId = owner->globalSettings()->cloudSystemId();
    auto authKey = owner->globalSettings()->cloudAuthKey();
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

        NX_LOGX(lm("Received error response from %1: %2").arg(nx::network::AppInfo::cloudName())
            .arg(api::toString(cdbResultCode)), cl_logWARNING);

        // Ignoring cloud error in detach operation.
        // So, it is allowed to perform detach while offline.
    }

    if (!m_cloudConnectionManager->detachSystemFromCloud())
    {
        NX_LOGX(lm("Cannot detach from cloud. Failed to reset cloud attributes. cloudSystemId %1")
            .arg(owner->globalSettings()->cloudSystemId()), cl_logDEBUG1);

        result.setError(
            QnJsonRestResult::CantProcessRequest,
            lit("Failed to save cloud credentials to local DB"));
        result.setReply(DetachFromCloudReply(
            DetachFromCloudReply::ResultCode::cannotCleanUpCloudDataInLocalDb));
        return nx_http::StatusCode::internalServerError;
    }

    NX_LOGX(lm("Successfully detached from cloud. cloudSystemId %1")
        .arg(owner->globalSettings()->cloudSystemId()), cl_logDEBUG2);

    return nx_http::StatusCode::ok;
}
