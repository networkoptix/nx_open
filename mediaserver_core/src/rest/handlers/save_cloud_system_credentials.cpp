/**********************************************************
* Sep 21, 2015
* a.kolesnikov
***********************************************************/

#include "save_cloud_system_credentials.h"

#include <cdb/connection.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <core/resource/param.h>
#include <core/resource/user_resource.h>
#include <utils/common/sync_call.h>
#include <utils/network/tcp_connection_priv.h>


int QnSaveCloudSystemCredentialsHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParams& params,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor*)
{
    const QString cloudSystemID = params.value(lit("cloudSystemID"));
    if (cloudSystemID.isEmpty())
    {
        result.setError(QnJsonRestResult::MissingParameter, lit("cloudSystemID"));
        return CODE_OK;
    }

    const QString cloudAuthKey = params.value(lit("cloudAuthKey"));
    if (cloudSystemID.isEmpty())
    {
        result.setError(QnJsonRestResult::MissingParameter, lit("cloudAuthKey"));
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

    //trying to connect to the cloud
    std::unique_ptr<
        nx::cdb::api::ConnectionFactory,
        decltype(&destroyConnectionFactory)
    > cloudConnectionFactory(createConnectionFactory(), &destroyConnectionFactory);
    auto cloudConnection = cloudConnectionFactory->createConnection(
        cloudSystemID.toStdString(),
        cloudAuthKey.toStdString());
    nx::cdb::api::ResultCode cdbResultCode = nx::cdb::api::ResultCode::ok;
    nx::cdb::api::ModuleInfo cloudModuleInfo;
    std::tie(cdbResultCode, cloudModuleInfo) = makeSyncCall<nx::cdb::api::ResultCode, nx::cdb::api::ModuleInfo>(
        std::bind(
            &nx::cdb::api::Connection::ping,
            cloudConnection.get(),
            std::placeholders::_1));

    if (cdbResultCode != nx::cdb::api::ResultCode::ok)
    {
        result.setError(
            QnJsonRestResult::CantProcessRequest,
            lit("Could not connect to cloud: %1").
                arg(QString::fromStdString(cloudConnectionFactory->toString(cdbResultCode))));
        return CODE_OK;
    }

    admin->setProperty("cloudSystemID", cloudSystemID);
    admin->setProperty("cloudAuthKey", cloudAuthKey);

    //TODO #ak check error code when propertyDictionary->saveParams supports it
    propertyDictionary->saveParams(admin->getId());

    result.setError(QnJsonRestResult::NoError);

    return CODE_OK;
}
