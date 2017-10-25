#include "vms_cloud_connection_processor.h"

#include <nx/utils/log/log.h>
#include <nx/utils/sync_call.h>

#include <nx/cloud/cdb/api/connection.h>
#include <nx/vms/utils/system_settings_processor.h>
#include <nx/vms/utils/vms_utils.h>

#include <api/global_settings.h>
#include <api/model/cloud_credentials_data.h>
#include <api/model/detach_from_cloud_data.h>
#include <api/model/detach_from_cloud_reply.h>
#include <api/model/setup_cloud_system_data.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <nx_ec/data/api_cloud_system_data.h>
#include <rest/server/json_rest_result.h>

#include "cloud_manager_group.h"

namespace nx {
namespace vms {
namespace cloud_integration {

VmsCloudConnectionProcessor::VmsCloudConnectionProcessor(
    QnCommonModule* commonModule,
    CloudManagerGroup* cloudManagerGroup)
    :
    m_commonModule(commonModule),
    m_cloudManagerGroup(cloudManagerGroup)
{
}

void VmsCloudConnectionProcessor::setSystemSettingsProcessor(
    nx::vms::utils::SystemSettingsProcessor* systemSettingsProcessor)
{
    m_systemSettingsProcessor = systemSettingsProcessor;
}

nx_http::StatusCode::Value VmsCloudConnectionProcessor::saveCloudSystemCredentials(
    const CloudCredentialsData& data,
    QnJsonRestResult* result)
{
    if (!validateInputData(data, result))
        return nx_http::StatusCode::badRequest;

    if (!checkInternetConnection(result))
        return nx_http::StatusCode::badRequest;

    if (!saveCloudData(data, result))
        return nx_http::StatusCode::internalServerError;

    // Trying to connect to the cloud to activate system.
    // We connect to the cloud after saving properties to DB because
    // if server activates system in cloud and then save it to DB,
    // crash can result in unsynchronized unrecoverable state: there
    // is some system in cloud, but system does not know its credentials
    // and there is no way to find them out.

    if (!fetchNecessaryDataFromCloud(data, result))
    {
        if (rollback())
            return nx_http::StatusCode::serviceUnavailable;
        else
            return nx_http::StatusCode::internalServerError;
    }

    result->setError(QnJsonRestResult::NoError);
    return nx_http::StatusCode::ok;
}

nx_http::StatusCode::Value VmsCloudConnectionProcessor::setupCloudSystem(
    const QnAuthSession& authSession,
    const SetupCloudSystemData& data,
    QnJsonRestResult* result)
{
    if (!m_commonModule->globalSettings()->isNewSystem())
    {
        result->setError(
            QnJsonRestResult::Forbidden,
            lit("This method is allowed at initial state only. Use 'api/detachFromSystem' method first."));
        return nx_http::StatusCode::forbidden;
    }

    QString newSystemName = data.systemName;
    if (newSystemName.isEmpty())
    {
        result->setError(QnJsonRestResult::MissingParameter, lit("Parameter 'systemName' must be provided."));
        return nx_http::StatusCode::badRequest;
    }

    if (!m_commonModule->resourcePool()->getResourceById<QnMediaServerResource>(m_commonModule->moduleGUID()))
    {
        result->setError(QnJsonRestResult::CantProcessRequest, lit("Internal server error."));
        return nx_http::StatusCode::internalServerError;
    }

    const auto systemNameBak = m_commonModule->globalSettings()->systemName();

    m_commonModule->globalSettings()->setSystemName(data.systemName);
    m_commonModule->globalSettings()->setLocalSystemId(QnUuid::createUuid());
    if (!m_commonModule->globalSettings()->synchronizeNowSync())
    {
        // Changing system name back.
        m_commonModule->globalSettings()->setSystemName(systemNameBak);
        m_commonModule->globalSettings()->setLocalSystemId(QnUuid()); //< revert
        result->setError(
            QnJsonRestResult::CantProcessRequest,
            lit("Internal server error."));
        return nx_http::StatusCode::internalServerError;
    }
    
    const nx_http::StatusCode::Value httpResult = saveCloudSystemCredentials(data, result);
    if (!nx_http::StatusCode::isSuccessCode(httpResult))
    {
        // Changing system name back.
        m_commonModule->globalSettings()->setSystemName(systemNameBak);
        m_commonModule->globalSettings()->setLocalSystemId(QnUuid()); //< Revert.
        return httpResult;
    }
    m_commonModule->globalSettings()->synchronizeNowSync();

    QString errStr;
    PasswordData adminPasswordData;
    // Reset admin password to random value to prevent NX1 root login via default password.
    adminPasswordData.password = QnUuid::createUuid().toString();
    if (!nx::vms::utils::updateUserCredentials(
            m_commonModule->ec2Connection(),
            adminPasswordData,
            QnOptionalBool(false),
            m_commonModule->resourcePool()->getAdministrator(),
            &errStr))
    {
        result->setError(QnJsonRestResult::CantProcessRequest, errStr);
        return nx_http::StatusCode::internalServerError;
    }

    std::unique_ptr<nx::vms::utils::SystemSettingsProcessor> defaultSystemSettingsProcessor;
    nx::vms::utils::SystemSettingsProcessor* systemSettingsProcessorToUse = nullptr;
    if (m_systemSettingsProcessor)
    {
        systemSettingsProcessorToUse = m_systemSettingsProcessor;
    }
    else
    {
        defaultSystemSettingsProcessor =
            std::make_unique<nx::vms::utils::SystemSettingsProcessor>(m_commonModule);
        systemSettingsProcessorToUse = defaultSystemSettingsProcessor.get();
    }

    if (!systemSettingsProcessorToUse->updateSettings(
            Qn::kSystemAccess,
            authSession,
            data.systemSettings,
            result))
    {
        NX_WARNING(this, lm("Failed to write system settings"));
    }

    return nx_http::StatusCode::ok;
}

nx_http::StatusCode::Value VmsCloudConnectionProcessor::detachFromCloud(
    const DetachFromCloudData& data,
    DetachFromCloudReply* result)
{
    if (!nx::vms::utils::validatePasswordData(data, &m_errorDescription))
    {
        NX_LOGX(lm("Cannot detach from cloud. Password check failed. cloudSystemId %1")
            .arg(m_commonModule->globalSettings()->cloudSystemId()), cl_logDEBUG1);
        *result = DetachFromCloudReply(
            DetachFromCloudReply::ResultCode::invalidPasswordData);
        return nx_http::StatusCode::forbidden;
    }

    auto adminUser = m_commonModule->resourcePool()->getAdministrator();
    const bool shouldResetSystemToNewState = !adminUser->isEnabled() && !data.hasPassword();

    if (shouldResetSystemToNewState)
    {
        NX_VERBOSE(this, lm("Resetting system to the \"new\" state"));
        if (!nx::vms::utils::resetSystemToStateNew(m_commonModule))
        {
            NX_LOGX(lm("Cannot detach from cloud. Failed to reset system to state new. cloudSystemId %1")
                .arg(m_commonModule->globalSettings()->cloudSystemId()), cl_logDEBUG1);
            *result = DetachFromCloudReply(
                DetachFromCloudReply::ResultCode::cannotUpdateUserCredentials);
            return nx_http::StatusCode::internalServerError;
        }
    }
    else
    {
        NX_VERBOSE(this, lm("Enabling admin user"));

        // First of all, enable admin user and changing its password
        // so that there is always a way to connect to the system.
        if (!nx::vms::utils::updateUserCredentials(
                m_commonModule->ec2Connection(),
                data,
                QnOptionalBool(true),
                adminUser,
                &m_errorDescription))
        {
            NX_LOGX(lm("Cannot detach from cloud. Failed to re-enable local admin. cloudSystemId %1")
                .arg(m_commonModule->globalSettings()->cloudSystemId()), cl_logDEBUG1);
            *result = DetachFromCloudReply(
                DetachFromCloudReply::ResultCode::cannotUpdateUserCredentials);
            return nx_http::StatusCode::internalServerError;
        }
    }

    // Second, updating data in cloud.
    nx::cdb::api::ResultCode cdbResultCode = nx::cdb::api::ResultCode::ok;
    auto systemId = m_commonModule->globalSettings()->cloudSystemId();
    auto authKey = m_commonModule->globalSettings()->cloudAuthKey();
    auto cloudConnection = m_cloudManagerGroup->connectionManager.getCloudConnection(systemId, authKey);
    std::tie(cdbResultCode) = makeSyncCall<nx::cdb::api::ResultCode>(
        std::bind(
            &nx::cdb::api::SystemManager::unbindSystem,
            cloudConnection->systemManager(),
            systemId.toStdString(),
            std::placeholders::_1));
    if (cdbResultCode != nx::cdb::api::ResultCode::ok)
    {
        // TODO: #ak: Rollback "admin" user modification?

        NX_LOGX(lm("Received error response from %1: %2").arg(nx::network::AppInfo::cloudName())
            .arg(nx::cdb::api::toString(cdbResultCode)), cl_logWARNING);

        // Ignoring cloud error in detach operation.
        // So, it is allowed to perform detach while offline.
    }

    if (!m_cloudManagerGroup->connectionManager.detachSystemFromCloud())
    {
        NX_LOGX(lm("Cannot detach from cloud. Failed to reset cloud attributes. cloudSystemId %1")
            .arg(m_commonModule->globalSettings()->cloudSystemId()), cl_logDEBUG1);

        m_errorDescription = lit("Failed to save cloud credentials to local DB");
        *result = DetachFromCloudReply(
            DetachFromCloudReply::ResultCode::cannotCleanUpCloudDataInLocalDb);
        return nx_http::StatusCode::internalServerError;
    }

    NX_LOGX(lm("Successfully detached from cloud. cloudSystemId %1")
        .arg(m_commonModule->globalSettings()->cloudSystemId()), cl_logDEBUG2);

    return nx_http::StatusCode::ok;
}

QString VmsCloudConnectionProcessor::errorDescription() const
{
    return m_errorDescription;
}

bool VmsCloudConnectionProcessor::validateInputData(
    const CloudCredentialsData& data,
    QnJsonRestResult* result)
{
    using namespace nx::settings_names;

    if (data.cloudSystemID.isEmpty())
    {
        NX_LOGX(lit("Missing required parameter CloudSystemID"), cl_logDEBUG1);
        result->setError(QnRestResult::ErrorDescriptor(
            QnJsonRestResult::MissingParameter, kNameCloudSystemId));
        return false;
    }

    if (data.cloudAuthKey.isEmpty())
    {
        NX_LOGX(lit("Missing required parameter CloudAuthKey"), cl_logDEBUG1);
        result->setError(QnRestResult::ErrorDescriptor(
            QnJsonRestResult::MissingParameter, kNameCloudAuthKey));
        return false;
    }

    if (data.cloudAccountName.isEmpty())
    {
        NX_LOGX(lit("Missing required parameter CloudAccountName"), cl_logDEBUG1);
        result->setError(QnRestResult::ErrorDescriptor(
            QnJsonRestResult::MissingParameter, kNameCloudAccountName));
        return false;
    }

    const QString cloudSystemId = m_commonModule->globalSettings()->cloudSystemId();
    if (!cloudSystemId.isEmpty() &&
        !m_commonModule->globalSettings()->cloudAuthKey().isEmpty())
    {
        NX_LOGX(lit("Attempt to bind to cloud already-bound system"), cl_logDEBUG1);
        result->setError(
            QnJsonRestResult::CantProcessRequest,
            QObject::tr("System already bound to cloud (id %1)").arg(cloudSystemId));
        return false;
    }

    return true;
}

bool VmsCloudConnectionProcessor::checkInternetConnection(
    QnJsonRestResult* result)
{
    auto server = m_commonModule->resourcePool()->getResourceById<QnMediaServerResource>(m_commonModule->moduleGUID());
    bool hasPublicIP = server && server->getServerFlags().testFlag(Qn::SF_HasPublicIP);
    if (!hasPublicIP)
    {
        result->setError(
            QnJsonRestResult::CantProcessRequest,
            QObject::tr("Server is not connected to the Internet."));
        NX_LOGX(result->errorString, cl_logWARNING);
        return false;
    }

    return true;
}

bool VmsCloudConnectionProcessor::saveCloudData(
    const CloudCredentialsData& data,
    QnJsonRestResult* result)
{
    if (!saveCloudCredentials(data, result))
        return false;

    if (!insertCloudOwner(data, result))
        return false;

    return true;
}

bool VmsCloudConnectionProcessor::saveCloudCredentials(
    const CloudCredentialsData& data,
    QnJsonRestResult* result)
{
    NX_LOGX(lm("Saving cloud credentials"), cl_logDEBUG1);

    m_commonModule->globalSettings()->setCloudSystemId(data.cloudSystemID);
    m_commonModule->globalSettings()->setCloudAccountName(data.cloudAccountName);
    m_commonModule->globalSettings()->setCloudAuthKey(data.cloudAuthKey);
    if (!m_commonModule->globalSettings()->synchronizeNowSync())
    {
        NX_LOGX(lit("Error saving cloud credentials to the local DB"), cl_logWARNING);
        result->setError(
            QnJsonRestResult::CantProcessRequest,
            QObject::tr("Failed to save cloud credentials to local DB"));
        return false;
    }

    return true;
}

bool VmsCloudConnectionProcessor::insertCloudOwner(
    const CloudCredentialsData& data,
    QnJsonRestResult* result)
{
    ::ec2::ApiUserData userData;
    userData.isCloud = true;
    userData.id = guidFromArbitraryData(data.cloudAccountName);
    userData.typeId = QnResourceTypePool::kUserTypeUuid;
    userData.email = data.cloudAccountName;
    userData.name = data.cloudAccountName;
    userData.permissions = Qn::GlobalAdminPermissionSet;
    userData.isAdmin = true;
    userData.isEnabled = true;
    userData.realm = nx::network::AppInfo::realm();
    userData.hash = ec2::ApiUserData::kCloudPasswordStub;
    userData.digest = ec2::ApiUserData::kCloudPasswordStub;

    const auto resultCode =
        m_commonModule->ec2Connection()
        ->getUserManager(Qn::kSystemAccess)->saveSync(userData);
    if (resultCode != ec2::ErrorCode::ok)
    {
        NX_LOGX(
            lm("Error inserting cloud owner to the local DB. %1").arg(resultCode),
            cl_logWARNING);
        result->setError(
            QnJsonRestResult::CantProcessRequest,
            QObject::tr("Failed to save cloud owner to local DB"));
        return false;
    }

    return true;
}

bool VmsCloudConnectionProcessor::fetchNecessaryDataFromCloud(
    const CloudCredentialsData& data,
    QnJsonRestResult* result)
{
    return saveLocalSystemIdToCloud(data, result) &&
        initializeCloudRelatedManagers(data, result);
}

bool VmsCloudConnectionProcessor::initializeCloudRelatedManagers(
    const CloudCredentialsData& /*data*/,
    QnJsonRestResult* result)
{
    using namespace nx::cdb;

    nx::cdb::api::ResultCode resultCode =
        m_cloudManagerGroup->authenticationNonceFetcher.initializeConnectionToCloudSync();
    if (resultCode != nx::cdb::api::ResultCode::ok)
    {
        NX_LOGX(lm("Failed to getch cloud nonce: %1")
            .arg(nx::cdb::api::toString(resultCode)), cl_logWARNING);
        result->setError(
            QnJsonRestResult::CantProcessRequest,
            QObject::tr("Could not connect to cloud: %1")
                .arg(QString::fromStdString(nx::cdb::api::toString(resultCode))));
        return false;
    }

    return true;
}

bool VmsCloudConnectionProcessor::saveLocalSystemIdToCloud(
    const CloudCredentialsData& data,
    QnJsonRestResult* result)
{
    ec2::ApiCloudSystemData opaque;
    opaque.localSystemId = m_commonModule->globalSettings()->localSystemId();

    nx::cdb::api::SystemAttributesUpdate systemAttributesUpdate;
    systemAttributesUpdate.systemId = data.cloudSystemID.toStdString();
    systemAttributesUpdate.opaque = QJson::serialized(opaque).toStdString();

    auto cloudConnection = m_cloudManagerGroup->connectionManager.getCloudConnection(
        data.cloudSystemID, data.cloudAuthKey);
    nx::cdb::api::ResultCode cdbResultCode = nx::cdb::api::ResultCode::ok;
    std::tie(cdbResultCode) =
        makeSyncCall<nx::cdb::api::ResultCode>(
            std::bind(
                &nx::cdb::api::SystemManager::update,
                cloudConnection->systemManager(),
                std::move(systemAttributesUpdate),
                std::placeholders::_1));
    if (cdbResultCode != nx::cdb::api::ResultCode::ok)
    {
        NX_LOGX(lm("Received error response from cloud: %1")
            .arg(nx::cdb::api::toString(cdbResultCode)), cl_logWARNING);
        result->setError(
            QnJsonRestResult::CantProcessRequest,
            QObject::tr("Could not connect to cloud: %1")
                .arg(QString::fromStdString(nx::cdb::api::toString(cdbResultCode))));
        return false;
    }

    return true;
}

bool VmsCloudConnectionProcessor::rollback()
{
    m_commonModule->globalSettings()->resetCloudParams();
    if (!m_commonModule->globalSettings()->synchronizeNowSync())
    {
        NX_LOGX(lit("Error resetting failed cloud credentials in the local DB"), cl_logWARNING);
        return false;
    }
    return true;
}

} // namespace cloud_integration
} // namespace vms
} // namespace nx
