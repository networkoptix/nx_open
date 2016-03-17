
#include "save_cloud_system_credentials.h"

#include <cdb/connection.h>

#include <api/model/cloud_credentials_data.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <core/resource/param.h>
#include <core/resource/user_resource.h>
#include <network/tcp_connection_priv.h>
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
    const auto admin = qnResPool->getAdministrator();
    if (data.reset)
    {
        if (!resetCloudParams())
            result.setError(
                QnJsonRestResult::CantProcessRequest,
                lit("Failed to save cloud credentials to local DB"));
        return CODE_OK;
    }

    if (data.cloudSystemID.isEmpty())
    {
        result.setError(QnJsonRestResult::MissingParameter, Qn::CLOUD_SYSTEM_ID);
        return CODE_OK;
    }

    if (data.cloudAuthKey.isEmpty())
    {
        result.setError(QnJsonRestResult::MissingParameter, Qn::CLOUD_SYSTEM_AUTH_KEY);
        return CODE_OK;
    }
    
    if (data.cloudAccountName.isEmpty())
    {
        result.setError(QnJsonRestResult::MissingParameter, Qn::CLOUD_ACCOUNT_NAME);
        return CODE_OK;
    }

    if (!admin->getProperty(Qn::CLOUD_SYSTEM_ID).isEmpty() &&
        !admin->getProperty(Qn::CLOUD_SYSTEM_AUTH_KEY).isEmpty())
    {
        result.setError(
            QnJsonRestResult::CantProcessRequest,
            lit("System already bound to cloud (id %1)").arg(admin->getProperty(Qn::CLOUD_SYSTEM_ID)));
        return CODE_OK;
    }

    admin->setProperty(Qn::CLOUD_SYSTEM_ID, data.cloudSystemID);
    admin->setProperty(Qn::CLOUD_SYSTEM_AUTH_KEY, data.cloudAuthKey);
    admin->setProperty(Qn::CLOUD_ACCOUNT_NAME, data.cloudAccountName);

    if (!propertyDictionary->saveParams(admin->getId()))
    {
        result.setError(
            QnJsonRestResult::CantProcessRequest,
            lit("Failed to save cloud credentials to local DB"));
        return CODE_OK;
    }

    //trying to connect to the cloud to activate system
    //we connect to the cloud after saving properties to DB because
    //if server activates system in cloud and then save it to DB, 
    //crash can result in unsynchronised unrecoverable state: there 
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
        admin->setProperty(Qn::CLOUD_SYSTEM_ID, QString());
        admin->setProperty(Qn::CLOUD_SYSTEM_AUTH_KEY, QString());
        if (!propertyDictionary->saveParams(admin->getId()))
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
        return CODE_OK;
    }

    result.setError(QnJsonRestResult::NoError);

    return CODE_OK;
}
