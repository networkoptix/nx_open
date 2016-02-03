#include "save_cloud_system_credentials.h"

#include <cdb/connection.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <core/resource/param.h>
#include <core/resource/user_resource.h>
#include <utils/common/sync_call.h>
#include <network/tcp_connection_priv.h>


namespace
{
    const QString UNBIND_PARAM_NAME = lit("unbind");
}

int QnSaveCloudSystemCredentialsHandler::executeGet(
    const QString& path,
    const QnRequestParams& params,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* connection)
{
#ifdef _DEBUG
    if (params.contains(UNBIND_PARAM_NAME))
        return executePost(path, params, QByteArray(), result, connection);
#else
    QN_UNUSED(path);
    QN_UNUSED(connection);
#endif

    const QString cloudSystemID = params.value(Qn::CLOUD_SYSTEM_ID);
    if (cloudSystemID.isEmpty())
    {
        result.setError(QnJsonRestResult::MissingParameter, Qn::CLOUD_SYSTEM_ID);
        return CODE_OK;
    }

    const QString cloudAuthKey = params.value(Qn::CLOUD_SYSTEM_AUTH_KEY);
    if (cloudSystemID.isEmpty())
    {
        result.setError(QnJsonRestResult::MissingParameter, Qn::CLOUD_SYSTEM_AUTH_KEY);
        return CODE_OK;
    }

    const auto admin = qnResPool->getAdministrator();

    if (!admin->getProperty(Qn::CLOUD_SYSTEM_ID).isEmpty() &&
        !admin->getProperty(Qn::CLOUD_SYSTEM_AUTH_KEY).isEmpty())
    {
        result.setError(
            QnJsonRestResult::CantProcessRequest,
            lit("System already bound to cloud (id %1)").arg(admin->getProperty(Qn::CLOUD_SYSTEM_ID)));
        return CODE_OK;
    }

    admin->setProperty(Qn::CLOUD_SYSTEM_ID, cloudSystemID);
    admin->setProperty(Qn::CLOUD_SYSTEM_AUTH_KEY, cloudAuthKey);

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
        cloudSystemID.toStdString(),
        cloudAuthKey.toStdString());
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

int QnSaveCloudSystemCredentialsHandler::executePost(
    const QString& /*path*/,
    const QnRequestParams& params,
    const QByteArray& /*body*/,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor*)
{
    if (!params.contains(UNBIND_PARAM_NAME))
    {
        result.setError(QnJsonRestResult::MissingParameter, UNBIND_PARAM_NAME);
        return CODE_OK;
    }

    const auto admin = qnResPool->getAdministrator();
    admin->setProperty(Qn::CLOUD_SYSTEM_ID, QString());
    admin->setProperty(Qn::CLOUD_SYSTEM_AUTH_KEY, QString());
    admin->setProperty(Qn::CLOUD_ACCOUNT_NAME, QString());
    propertyDictionary->saveParams(admin->getId());

    result.setError(QnJsonRestResult::NoError);

    return CODE_OK;
}
