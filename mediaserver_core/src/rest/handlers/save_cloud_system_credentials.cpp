
#include "save_cloud_system_credentials.h"

#include <cdb/connection.h>

#include <api/model/cloud_credentials_data.h>
#include <api/global_settings.h>

#include <nx/network/http/httptypes.h>

#include <utils/common/sync_call.h>
#include <media_server/serverutil.h>

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
    if (data.reset)
    {
        qnGlobalSettings->resetCloudParams();
        if (!qnGlobalSettings->synchronizeNowSync())
            result.setError(
                QnJsonRestResult::CantProcessRequest,
                lit("Failed to save cloud credentials to local DB"));
        return nx_http::StatusCode::ok;
    }

    if (data.cloudSystemID.isEmpty())
    {
        result.setError(QnJsonRestResult::MissingParameter, QnGlobalSettings::kNameCloudSystemID);
        return nx_http::StatusCode::invalidParameter;
    }

    if (data.cloudAuthKey.isEmpty())
    {
        result.setError(QnJsonRestResult::MissingParameter, QnGlobalSettings::kNameCloudAuthKey);
        return nx_http::StatusCode::invalidParameter;
    }

    if (data.cloudAccountName.isEmpty())
    {
        result.setError(QnJsonRestResult::MissingParameter, QnGlobalSettings::kNameCloudAccountName);
        return nx_http::StatusCode::invalidParameter;
    }

    const QString cloudSystemId = qnGlobalSettings->cloudSystemID();
    if (!cloudSystemId.isEmpty() &&
        !qnGlobalSettings->cloudAuthKey().isEmpty())
    {
        result.setError(
            QnJsonRestResult::CantProcessRequest,
            lit("System already bound to cloud (id %1)").arg(cloudSystemId));
        return nx_http::StatusCode::notAcceptable;
    }

    qnGlobalSettings->setCloudSystemID(data.cloudSystemID);
    qnGlobalSettings->setCloudAccountName(data.cloudAccountName);
    qnGlobalSettings->setCloudAuthKey(data.cloudAuthKey);
    if (!qnGlobalSettings->synchronizeNowSync())
    {
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
        nx::cdb::api::ConnectionFactory,
        decltype(&destroyConnectionFactory)
    > cloudConnectionFactory(createConnectionFactory(), &destroyConnectionFactory);
    auto cloudConnection = cloudConnectionFactory->createConnection(
        data.cloudSystemID.toStdString(),
        data.cloudAuthKey.toStdString());
    nx::cdb::api::ResultCode cdbResultCode = nx::cdb::api::ResultCode::ok;
    nx::cdb::api::NonceData nonceData;
    std::tie(cdbResultCode, nonceData) =
        makeSyncCall<nx::cdb::api::ResultCode, nx::cdb::api::NonceData>(
            std::bind(
                &nx::cdb::api::AuthProvider::getCdbNonce,
                cloudConnection->authProvider(),
                std::placeholders::_1));

    if (cdbResultCode != nx::cdb::api::ResultCode::ok)
    {
        //rolling back changes
        qnGlobalSettings->resetCloudParams();
        if (!qnGlobalSettings->synchronizeNowSync())
        {
            //we saved cloud credentials to local DB but failed to connect to cloud and could not remove them!
            //let's pretend we have registered successfully:
            //hopefully, cloud will become available some time later
            result.setError(QnJsonRestResult::NoError);
        }
        else
        {
            result.setError(
                QnJsonRestResult::CantProcessRequest,
                lit("Could not connect to cloud: %1").
                arg(QString::fromStdString(cloudConnectionFactory->toString(cdbResultCode))));
        }
        return nx_http::StatusCode::internalServerError;
    }

    result.setError(QnJsonRestResult::NoError);

    return nx_http::StatusCode::ok;
}
