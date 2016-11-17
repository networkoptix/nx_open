
#include "save_cloud_system_credentials.h"

#include <cdb/connection.h>

#include <nx/network/http/httptypes.h>
#include <nx/utils/log/log.h>
#include <nx/utils/string.h>

#include <api/model/cloud_credentials_data.h>
#include <api/global_settings.h>
#include <media_server/serverutil.h>
#include <utils/common/sync_call.h>

#include <nx_ec/data/api_cloud_system_data.h>
#include <nx/fusion/model_functions.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <common/common_module.h>
#include <cloud/cloud_connection_manager.h>

#include <rest/helpers/permissions_helper.h>
#include <rest/server/rest_connection_processor.h>


QnSaveCloudSystemCredentialsHandler::QnSaveCloudSystemCredentialsHandler(
    const CloudConnectionManager& cloudConnectionManager)
:
    m_cloudConnectionManager(cloudConnectionManager)
{
}

int QnSaveCloudSystemCredentialsHandler::executePost(
    const QString& /*path*/,
    const QnRequestParams& /*params*/,
    const QByteArray& body,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    const CloudCredentialsData data = QJson::deserialized<CloudCredentialsData>(body);
    return execute(data, result, owner);
}

int QnSaveCloudSystemCredentialsHandler::execute(
    const CloudCredentialsData& data,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    using namespace nx::cdb;
    using namespace nx::settings_names;

    if (QnPermissionsHelper::isSafeMode())
        return QnPermissionsHelper::safeModeError(result);
    if (!QnPermissionsHelper::hasOwnerPermissions(owner->accessRights()))
        return QnPermissionsHelper::notOwnerError(result);

    NX_LOGX(lm("Saving cloud credentials"), cl_logDEBUG1);

    if (data.cloudSystemID.isEmpty())
    {
        NX_LOGX(lit("Missing required parameter CloudSystemID"), cl_logDEBUG1);
        result.setError(QnRestResult::ErrorDescriptor(
            QnJsonRestResult::MissingParameter, kNameCloudSystemId));
        return nx_http::StatusCode::ok;
    }

    if (data.cloudAuthKey.isEmpty())
    {
        NX_LOGX(lit("Missing required parameter CloudAuthKey"), cl_logDEBUG1);
        result.setError(QnRestResult::ErrorDescriptor(
            QnJsonRestResult::MissingParameter, kNameCloudAuthKey));
        return nx_http::StatusCode::ok;
    }

    if (data.cloudAccountName.isEmpty())
    {
        NX_LOGX(lit("Missing required parameter CloudAccountName"), cl_logDEBUG1);
        result.setError(QnRestResult::ErrorDescriptor(
            QnJsonRestResult::MissingParameter, kNameCloudAccountName));
        return nx_http::StatusCode::ok;
    }

    const QString cloudSystemId = qnGlobalSettings->cloudSystemId();
    if (!cloudSystemId.isEmpty() &&
        !qnGlobalSettings->cloudAuthKey().isEmpty())
    {
        NX_LOGX(lit("Attempt to bind to cloud already-bound system"), cl_logDEBUG1);
        result.setError(
            QnJsonRestResult::CantProcessRequest,
            tr("System already bound to cloud (id %1)").arg(cloudSystemId));
        return nx_http::StatusCode::ok;
    }

    auto server = qnResPool->getResourceById<QnMediaServerResource>(qnCommon->moduleGUID());
    bool hasPublicIP = server && server->getServerFlags().testFlag(Qn::SF_HasPublicIP);
    if (!hasPublicIP)
    {
        result.setError(
            QnJsonRestResult::CantProcessRequest,
            tr("Server is not connected to the Internet."));
        NX_LOGX(result.errorString, cl_logWARNING);
        return nx_http::StatusCode::internalServerError;
    }

    qnGlobalSettings->setCloudSystemId(data.cloudSystemID);
    qnGlobalSettings->setCloudAccountName(data.cloudAccountName);
    qnGlobalSettings->setCloudAuthKey(data.cloudAuthKey);
    if (!qnGlobalSettings->synchronizeNowSync())
    {
        NX_LOGX(lit("Error saving cloud credentials to the local DB"), cl_logWARNING);
        result.setError(
             QnJsonRestResult::CantProcessRequest,
             tr("Failed to save cloud credentials to local DB"));
         return nx_http::StatusCode::internalServerError;
    }

    //trying to connect to the cloud to activate system
    //we connect to the cloud after saving properties to DB because
    //if server activates system in cloud and then save it to DB,
    //crash can result in unsynchronized unrecoverable state: there
    //is some system in cloud, but system does not know its credentials
    //and there is no way to find them out

    ec2::ApiCloudSystemData opaque;
    opaque.localSystemId = qnGlobalSettings->localSystemId();

    api::SystemAttributesUpdate systemAttributesUpdate;
    systemAttributesUpdate.systemId = data.cloudSystemID.toStdString();
    systemAttributesUpdate.opaque = QJson::serialized(opaque).toStdString();

    auto cloudConnection = m_cloudConnectionManager.getCloudConnection(
        data.cloudSystemID, data.cloudAuthKey);
    api::ResultCode cdbResultCode = api::ResultCode::ok;
    std::tie(cdbResultCode) =
        makeSyncCall<api::ResultCode>(
            std::bind(
                &api::SystemManager::update,
                cloudConnection->systemManager(),
                std::move(systemAttributesUpdate),
                std::placeholders::_1));

    if (cdbResultCode != api::ResultCode::ok)
    {
        NX_LOGX(lm("Received error response from cloud: %1")
            .arg(api::toString(cdbResultCode)), cl_logWARNING);

        //rolling back changes
        qnGlobalSettings->resetCloudParams();
        if (!qnGlobalSettings->synchronizeNowSync())
        {
            NX_LOGX(lit("Error resetting failed cloud credentials in the local DB"), cl_logWARNING);
            //we saved cloud credentials to local DB but failed to connect to cloud and could not remove them!
            //let's pretend we have registered successfully:
            //hopefully, cloud will become available some time later
            result.setError(QnJsonRestResult::NoError);
            return nx_http::StatusCode::internalServerError;
        }
        else
        {
            result.setError(
                QnJsonRestResult::CantProcessRequest,
                tr("Could not connect to cloud: %1").
                    arg(QString::fromStdString(nx::cdb::api::toString(cdbResultCode))));
            return nx_http::StatusCode::ok;
        }
    }

    result.setError(QnJsonRestResult::NoError);

    return nx_http::StatusCode::ok;
}
