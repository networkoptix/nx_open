
#include "save_cloud_system_credentials.h"

#include <cdb/connection.h>

#include <nx/network/http/httptypes.h>
#include <nx/utils/log/log.h>

#include <api/model/cloud_credentials_data.h>
#include <api/global_settings.h>
#include <media_server/serverutil.h>
#include <utils/common/sync_call.h>
#include <utils/common/model_functions.h>


int QnSaveCloudSystemCredentialsHandler::executePost(
    const QString& /*path*/,
    const QnRequestParams& /*params*/,
    const QByteArray& body,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor*)
{
    const CloudCredentialsData data = QJson::deserialized<CloudCredentialsData>(body);
    return execute(data, result);
}

int QnSaveCloudSystemCredentialsHandler::execute(
    const CloudCredentialsData& data,
    QnJsonRestResult& result)
{
    using namespace nx::cdb;

    NX_LOGX(lm("%1 cloud credentials").arg(data.reset ? "Resetting" : "Saving"),
        cl_logDEBUG1);

    if (data.reset)
    {
        qnGlobalSettings->resetCloudParams();
        if (!qnGlobalSettings->synchronizeNowSync())
        {
            NX_LOGX(lit("Error resetting cloud credentials in local DB"), cl_logWARNING);
            result.setError(
                QnJsonRestResult::CantProcessRequest,
                lit("Failed to save cloud credentials to local DB"));
            return nx_http::StatusCode::internalServerError;
        }
        return nx_http::StatusCode::ok;
    }

    if (data.cloudSystemID.isEmpty())
    {
        NX_LOGX(lit("Missing required parameter CloudSystemID"), cl_logDEBUG1);
        result.setError(QnRestResult::ErrorDescriptor(
            QnJsonRestResult::MissingParameter, QnGlobalSettings::kNameCloudSystemID));
        return nx_http::StatusCode::ok;
    }

    if (data.cloudAuthKey.isEmpty())
    {
        NX_LOGX(lit("Missing required parameter CloudAuthKey"), cl_logDEBUG1);
        result.setError(QnRestResult::ErrorDescriptor(
            QnJsonRestResult::MissingParameter, QnGlobalSettings::kNameCloudAuthKey));
        return nx_http::StatusCode::ok;
    }

    if (data.cloudAccountName.isEmpty())
    {
        NX_LOGX(lit("Missing required parameter CloudAccountName"), cl_logDEBUG1);
        result.setError(QnRestResult::ErrorDescriptor(
            QnJsonRestResult::MissingParameter, QnGlobalSettings::kNameCloudAccountName));
        return nx_http::StatusCode::ok;
    }

    const QString cloudSystemId = qnGlobalSettings->cloudSystemID();
    if (!cloudSystemId.isEmpty() &&
        !qnGlobalSettings->cloudAuthKey().isEmpty())
    {
        NX_LOGX(lit("Attempt to bind to cloud already-bound system"), cl_logDEBUG1);
        result.setError(
            QnJsonRestResult::CantProcessRequest,
            lit("System already bound to cloud (id %1)").arg(cloudSystemId));
        return nx_http::StatusCode::ok;
    }

    qnGlobalSettings->setCloudSystemID(data.cloudSystemID);
    qnGlobalSettings->setCloudAccountName(data.cloudAccountName);
    qnGlobalSettings->setCloudAuthKey(data.cloudAuthKey);
    if (!qnGlobalSettings->synchronizeNowSync())
    {
        NX_LOGX(lit("Error saving cloud credentials to the local DB"), cl_logWARNING);
        result.setError(
             QnJsonRestResult::CantProcessRequest,
             lit("Failed to save cloud credentials to local DB"));
         return nx_http::StatusCode::internalServerError;
    }

    //trying to connect to the cloud to activate system
    //we connect to the cloud after saving properties to DB because
    //if server activates system in cloud and then save it to DB,
    //crash can result in unsynchronized unrecoverable state: there
    //is some system in cloud, but system does not know its credentials
    //and there is no way to find them out
    std::unique_ptr<
        api::ConnectionFactory,
        decltype(&destroyConnectionFactory)
    > cloudConnectionFactory(createConnectionFactory(), &destroyConnectionFactory);
    auto cloudConnection = cloudConnectionFactory->createConnection(
        data.cloudSystemID.toStdString(),
        data.cloudAuthKey.toStdString());
    api::ResultCode cdbResultCode = api::ResultCode::ok;
    api::NonceData nonceData;
    std::tie(cdbResultCode, nonceData) =
        makeSyncCall<api::ResultCode, api::NonceData>(
            std::bind(
                &api::AuthProvider::getCdbNonce,
                cloudConnection->authProvider(),
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
                lit("Could not connect to cloud: %1").
                arg(QString::fromStdString(cloudConnectionFactory->toString(cdbResultCode))));
            return nx_http::StatusCode::ok;
        }
    }

    result.setError(QnJsonRestResult::NoError);

    return nx_http::StatusCode::ok;
}
