#include "vms_utils.h"

#include <QtCore/QFile>

#include <nx/utils/log/log.h>

#include <api/global_settings.h>
#include <api/model/configure_system_data.h>
#include <api/resource_property_adaptor.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <common/common_module.h>
#include <nx_ec/data/api_conversion_functions.h>

namespace nx {
namespace vms {
namespace utils {

bool backupDatabase(
    const QString& outputDir,
    std::shared_ptr<ec2::AbstractECConnection> connection)
{
    QString dir = outputDir + lit("/");
    QString fileName;
    for (int i = -1; ; i++)
    {
        QString suffix = (i < 0) ? QString() : lit("_") + QString::number(i);
        fileName = dir + lit("ecs") + suffix + lit(".backup");
        if (!QFile::exists(fileName))
            break;
    }

    const ec2::ErrorCode errorCode = connection->dumpDatabaseToFileSync(fileName);
    if (errorCode != ec2::ErrorCode::ok)
    {
        NX_LOG(lit("Failed to dump EC database: %1").arg(ec2::toString(errorCode)), cl_logERROR);
        return false;
    }

    return true;
}

bool configureLocalPeerAsPartOfASystem(
    QnCommonModule* commonModule,
    const ConfigureSystemData& data)
{
    if (commonModule->globalSettings()->localSystemId() == data.localSystemId)
        return true;

    QnMediaServerResourcePtr server =
        commonModule->resourcePool()->getResourceById<QnMediaServerResource>(
            commonModule->moduleGUID());
    if (!server)
    {
        NX_LOG("Cannot find self server resource!", cl_logERROR);
        return false;
    }

    auto connection = commonModule->ec2Connection();

    // add foreign users
    for (const auto& user : data.foreignUsers)
    {
        if (connection->getUserManager(Qn::kSystemAccess)->saveSync(user) != ec2::ErrorCode::ok)
            return false;
    }

    // add foreign resource params
    if (connection->getResourceManager(Qn::kSystemAccess)->saveSync(data.additionParams) != ec2::ErrorCode::ok)
        return false;

    // apply remote settings
    const auto& settings = commonModule->globalSettings()->allSettings();
    for (const auto& foreignSetting : data.foreignSettings)
    {
        for (QnAbstractResourcePropertyAdaptor* setting : settings)
        {
            if (setting->key() == foreignSetting.name)
            {
                setting->setSerializedValue(foreignSetting.value);
                break;
            }
        }
    }

    commonModule->setSystemIdentityTime(data.sysIdTime, commonModule->moduleGUID());

    if (data.localSystemId.isNull())
        commonModule->globalSettings()->resetCloudParams();

    if (!data.systemName.isEmpty())
        commonModule->globalSettings()->setSystemName(data.systemName);

    commonModule->globalSettings()->setLocalSystemId(data.localSystemId);
    commonModule->globalSettings()->synchronizeNowSync();

    connection->setTransactionLogTime(data.tranLogTime);

    // update auth key if system name is changed
    server->setAuthKey(QnUuid::createUuid().toString());
    ec2::ApiMediaServerData apiServer;
    fromResourceToApi(server, apiServer);
    if (connection->getMediaServerManager(Qn::kSystemAccess)->saveSync(apiServer) != ec2::ErrorCode::ok)
    {
        NX_LOG("Failed to update server auth key while configuring system", cl_logWARNING);
    }

    if (!data.foreignServer.id.isNull())
    {
        // add foreign server to pass auth if admin user is disabled
        if (connection->getMediaServerManager(Qn::kSystemAccess)->saveSync(data.foreignServer) != ec2::ErrorCode::ok)
        {
            NX_LOG("Failed to add foreign server while configuring system", cl_logWARNING);
        }
    }

    return true;
}

} // namespace utils
} // namespace vms
} // namespace nx
