#include "vms_utils.h"

#include <QtCore/QFile>

#include <nx/utils/crypt/linux_passwd_crypt.h>
#include <nx/utils/log/log.h>

#include <api/global_settings.h>
#include <api/model/configure_system_data.h>
#include <api/resource_property_adaptor.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <common/common_module.h>
#include <network/system_helpers.h>
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

bool validatePasswordData(const PasswordData& passwordData, QString* errStr)
{
    if (errStr)
        errStr->clear();

    if (!(passwordData.passwordHash.isEmpty() == passwordData.realm.isEmpty() &&
        passwordData.passwordDigest.isEmpty() == passwordData.realm.isEmpty() &&
        passwordData.cryptSha512Hash.isEmpty() == passwordData.realm.isEmpty()))
    {
        // These values MUST be all filled or all NOT filled.
        NX_LOG(lit("All password hashes MUST be supplied all together along with realm"), cl_logDEBUG2);

        if (errStr)
            *errStr = lit("All password hashes MUST be supplied all together along with realm");
        return false;
    }

    return true;
}

bool updateUserCredentials(
    std::shared_ptr<ec2::AbstractECConnection> connection,
    PasswordData data,
    QnOptionalBool isEnabled,
    const QnUserResourcePtr& userRes,
    QString* errString,
    QnUserResourcePtr* outUpdatedUser)
{
    if (!userRes)
    {
        if (errString)
            *errString = lit("Temporary unavailable. Please try later.");
        return false;
    }

    ec2::ApiUserData apiOldUser;
    fromResourceToApi(userRes, apiOldUser);

    //generating cryptSha512Hash
    if (data.cryptSha512Hash.isEmpty() && !data.password.isEmpty())
        data.cryptSha512Hash = linuxCryptSha512(data.password.toUtf8(), generateSalt(LINUX_CRYPT_SALT_LENGTH));

    //making copy of admin user to be able to rollback local changed on DB update failure
    QnUserResourcePtr updatedUser = QnUserResourcePtr(new QnUserResource(*userRes));
    if (outUpdatedUser)
        *outUpdatedUser = updatedUser;

    if (data.password.isEmpty() &&
        updatedUser->getHash() == data.passwordHash &&
        updatedUser->getDigest() == data.passwordDigest &&
        updatedUser->getCryptSha512Hash() == data.cryptSha512Hash &&
        (!isEnabled.isDefined() || updatedUser->isEnabled() == isEnabled.value()))
    {
        //no need to update anything
        return true;
    }

    if (isEnabled.isDefined())
        updatedUser->setEnabled(isEnabled.value());

    if (!data.password.isEmpty())
    {
        /* set new password */
        updatedUser->setPasswordAndGenerateHash(data.password);
    }
    else if (!data.passwordHash.isEmpty())
    {
        updatedUser->setRealm(data.realm);
        updatedUser->setHash(data.passwordHash);
        updatedUser->setDigest(data.passwordDigest);
        if (!data.cryptSha512Hash.isEmpty())
            updatedUser->setCryptSha512Hash(data.cryptSha512Hash);
    }

    ec2::ApiUserData apiUser;
    fromResourceToApi(updatedUser, apiUser);

    if (apiOldUser == apiUser)
        return true; //< Nothing to update.

    auto errCode = connection->getUserManager(Qn::kSystemAccess)->saveSync(apiUser, data.password);
    NX_ASSERT(errCode != ec2::ErrorCode::forbidden, "Access check should be implemented before");
    if (errCode != ec2::ErrorCode::ok)
    {
        if (errString)
            *errString = lit("Internal server database error: %1").arg(toString(errCode));
        return false;
    }
    updatedUser->resetPassword();
    return true;
}

bool resetSystemToStateNew(QnCommonModule* commonModule)
{
    NX_LOG(lit("Resetting system to the \"new\" state"), cl_logINFO);

    commonModule->globalSettings()->setLocalSystemId(QnUuid());   //< Marking system as a "new".
    if (!commonModule->globalSettings()->synchronizeNowSync())
    {
        NX_LOG(lit("Error saving changes to global settings"), cl_logINFO);
        return false;
    }

    auto adminUserResource = commonModule->resourcePool()->getAdministrator();
    PasswordData data;
    data.password = helpers::kFactorySystemPassword;
    return updateUserCredentials(
        commonModule->ec2Connection(),
        data,
        QnOptionalBool(true),
        adminUserResource);
}

} // namespace utils
} // namespace vms
} // namespace nx
