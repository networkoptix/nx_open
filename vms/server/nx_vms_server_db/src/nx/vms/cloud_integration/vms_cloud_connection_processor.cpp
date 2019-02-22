#include "vms_cloud_connection_processor.h"

#include <api/global_settings.h>
#include <api/model/cloud_credentials_data.h>
#include <api/model/detach_from_cloud_data.h>
#include <api/model/detach_from_cloud_reply.h>
#include <api/model/setup_cloud_system_data.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <rest/server/json_rest_result.h>

#include <nx/cloud/db/api/connection.h>
#include <nx/utils/log/log.h>
#include <nx/utils/sync_call.h>
#include <nx/vms/api/data/cloud_system_data.h>
#include <nx/vms/utils/system_settings_processor.h>
#include <nx/vms/utils/vms_utils.h>

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

nx::network::http::StatusCode::Value VmsCloudConnectionProcessor::bindSystemToCloud(
    const CloudCredentialsData& data,
    QnJsonRestResult* result)
{
    if (!validateInputData(data, result))
        return nx::network::http::StatusCode::badRequest;

    if (!checkInternetConnection(result))
        return nx::network::http::StatusCode::badRequest;

    if (!saveCloudData(data, result))
        return nx::network::http::StatusCode::internalServerError;

    // Trying to connect to the cloud to activate system.
    // We connect to the cloud after saving properties to DB because
    // if server activates system in cloud and then save it to DB,
    // crash can result in unsynchronized unrecoverable state: there
    // is some system in cloud, but system does not know its credentials
    // and there is no way to find them out.

    if (!fetchNecessaryDataFromCloud(data, result))
    {
        if (rollback())
            return nx::network::http::StatusCode::serviceUnavailable;
        else
            return nx::network::http::StatusCode::internalServerError;
    }

    result->setError(QnJsonRestResult::NoError);
    return nx::network::http::StatusCode::ok;
}

nx::network::http::StatusCode::Value VmsCloudConnectionProcessor::setupCloudSystem(
    const QnAuthSession& authSession,
    const SetupCloudSystemData& data,
    QnJsonRestResult* result)
{
    if (!m_commonModule->globalSettings()->isNewSystem())
    {
        result->setError(
            QnJsonRestResult::Forbidden,
            "This method is allowed at initial state only. Use 'api/detachFromSystem' method first.");
        return nx::network::http::StatusCode::forbidden;
    }

    QString newSystemName = data.systemName;
    if (newSystemName.isEmpty())
    {
        result->setError(QnJsonRestResult::MissingParameter,
			"Parameter 'systemName' must be provided.");
        return nx::network::http::StatusCode::badRequest;
    }

    if (!m_commonModule->resourcePool()->getResourceById<QnMediaServerResource>(m_commonModule->moduleGUID()))
    {
        result->setError(QnJsonRestResult::CantProcessRequest, "Internal server error.");
        return nx::network::http::StatusCode::internalServerError;
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
            "Internal server error.");
        return nx::network::http::StatusCode::internalServerError;
    }

    const nx::network::http::StatusCode::Value httpResult = bindSystemToCloud(data, result);
    if (!nx::network::http::StatusCode::isSuccessCode(httpResult))
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
        return nx::network::http::StatusCode::internalServerError;
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

    return nx::network::http::StatusCode::ok;
}

nx::network::http::StatusCode::Value VmsCloudConnectionProcessor::detachFromCloud(
    const DetachFromCloudData& data,
    DetachFromCloudReply* result)
{
    if (!nx::vms::utils::validatePasswordData(data, &m_errorDescription))
    {
        NX_DEBUG(this, lm("Cannot detach from cloud. Password check failed. cloudSystemId %1")
            .arg(m_commonModule->globalSettings()->cloudSystemId()));
        *result = DetachFromCloudReply(
            DetachFromCloudReply::ResultCode::invalidPasswordData);
        return nx::network::http::StatusCode::forbidden;
    }

    auto adminUser = m_commonModule->resourcePool()->getAdministrator();
    const bool shouldResetSystemToNewState = !adminUser->isEnabled() && !data.hasPassword();

    if (shouldResetSystemToNewState)
    {
        NX_VERBOSE(this, lm("Resetting system to the \"new\" state"));
        if (!nx::vms::utils::resetSystemToStateNew(m_commonModule))
        {
            NX_DEBUG(this, lm("Cannot detach from cloud. Failed to reset system to state new. cloudSystemId %1")
                .arg(m_commonModule->globalSettings()->cloudSystemId()));
            *result = DetachFromCloudReply(
                DetachFromCloudReply::ResultCode::cannotUpdateUserCredentials);
            return nx::network::http::StatusCode::internalServerError;
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
            NX_DEBUG(this, lm("Cannot detach from cloud. Failed to re-enable local admin. cloudSystemId %1")
                .arg(m_commonModule->globalSettings()->cloudSystemId()));
            *result = DetachFromCloudReply(
                DetachFromCloudReply::ResultCode::cannotUpdateUserCredentials);
            return nx::network::http::StatusCode::internalServerError;
        }
    }

    // Second, updating data in cloud.
    nx::cloud::db::api::ResultCode cdbResultCode = nx::cloud::db::api::ResultCode::ok;
    auto systemId = m_commonModule->globalSettings()->cloudSystemId();
    auto authKey = m_commonModule->globalSettings()->cloudAuthKey();
    auto cloudConnection = m_cloudManagerGroup->connectionManager.getCloudConnection(systemId, authKey);
    std::tie(cdbResultCode) = makeSyncCall<nx::cloud::db::api::ResultCode>(
        std::bind(
            &nx::cloud::db::api::SystemManager::unbindSystem,
            cloudConnection->systemManager(),
            systemId.toStdString(),
            std::placeholders::_1));
    if (cdbResultCode != nx::cloud::db::api::ResultCode::ok)
    {
        // TODO: #ak: Rollback "admin" user modification?

        NX_WARNING(this, lm("Received error response from %1: %2").arg(nx::network::AppInfo::cloudName())
            .arg(nx::cloud::db::api::toString(cdbResultCode)));

        // Ignoring cloud error in detach operation.
        // So, it is allowed to perform detach while offline.
    }

    if (!m_cloudManagerGroup->connectionManager.detachSystemFromCloud())
    {
        NX_DEBUG(this, lm("Cannot detach from cloud. Failed to reset cloud attributes. cloudSystemId %1")
            .arg(m_commonModule->globalSettings()->cloudSystemId()));

        m_errorDescription = QString("Failed to save %1 credentials to local DB")
            .arg(nx::network::AppInfo::cloudName());
        *result = DetachFromCloudReply(
            DetachFromCloudReply::ResultCode::cannotCleanUpCloudDataInLocalDb);
        return nx::network::http::StatusCode::internalServerError;
    }

    NX_VERBOSE(this, lm("Successfully detached from cloud. cloudSystemId %1")
        .arg(m_commonModule->globalSettings()->cloudSystemId()));

    return nx::network::http::StatusCode::ok;
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
        NX_DEBUG(this, "Missing required parameter CloudSystemID");
        result->setError(QnRestResult::ErrorDescriptor(
            QnJsonRestResult::MissingParameter, kNameCloudSystemId));
        return false;
    }

    if (data.cloudAuthKey.isEmpty())
    {
        NX_DEBUG(this, "Missing required parameter CloudAuthKey");
        result->setError(QnRestResult::ErrorDescriptor(
            QnJsonRestResult::MissingParameter, kNameCloudAuthKey));
        return false;
    }

    if (data.cloudAccountName.isEmpty())
    {
        NX_DEBUG(this, "Missing required parameter CloudAccountName");
        result->setError(QnRestResult::ErrorDescriptor(
            QnJsonRestResult::MissingParameter, kNameCloudAccountName));
        return false;
    }

    const QString cloudSystemId = m_commonModule->globalSettings()->cloudSystemId();
    if (!cloudSystemId.isEmpty() &&
        !m_commonModule->globalSettings()->cloudAuthKey().isEmpty())
    {
        NX_DEBUG(this, "Attempt to bind to cloud already-bound system");
        result->setError(
            QnJsonRestResult::CantProcessRequest,
            QString("System is already bound to %1 (id %2)").arg(
				nx::network::AppInfo::cloudName(),
				cloudSystemId));
        return false;
    }

    return true;
}

bool VmsCloudConnectionProcessor::checkInternetConnection(
    QnJsonRestResult* result)
{
    auto server = m_commonModule->resourcePool()->getResourceById<QnMediaServerResource>(
        m_commonModule->moduleGUID());
    bool hasPublicIP = server && server->getServerFlags().testFlag(nx::vms::api::SF_HasPublicIP);
    if (!hasPublicIP)
    {
        result->setError(
            QnJsonRestResult::CantProcessRequest,
            "Server is not connected to the Internet.");
        NX_WARNING(this, result->errorString);
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
    NX_DEBUG(this, lm("Saving cloud credentials"));

    m_commonModule->globalSettings()->setCloudSystemId(data.cloudSystemID);
    m_commonModule->globalSettings()->setCloudAccountName(data.cloudAccountName);
    m_commonModule->globalSettings()->setCloudAuthKey(data.cloudAuthKey);
    if (!m_commonModule->globalSettings()->synchronizeNowSync())
    {
        NX_WARNING(this, "Error saving cloud credentials to the local DB");
        result->setError(
            QnJsonRestResult::CantProcessRequest,
            QString("Failed to save %1 credentials to local DB")
			    .arg(nx::network::AppInfo::cloudName()));
        return false;
    }

    return true;
}

bool VmsCloudConnectionProcessor::insertCloudOwner(
    const CloudCredentialsData& data,
    QnJsonRestResult* result)
{
    nx::vms::api::UserData userData;
    userData.isCloud = true;
    userData.id = guidFromArbitraryData(data.cloudAccountName);
    userData.email = data.cloudAccountName;
    userData.name = data.cloudAccountName;
    userData.permissions = GlobalPermission::adminPermissions;
    userData.isAdmin = true;
    userData.isEnabled = true;
    userData.realm = nx::network::AppInfo::realm();
    userData.hash = nx::vms::api::UserData::kCloudPasswordStub;
    userData.digest = nx::vms::api::UserData::kCloudPasswordStub;

    const auto resultCode =
        m_commonModule->ec2Connection()
        ->getUserManager(Qn::kSystemAccess)->saveSync(userData);
    if (resultCode != ec2::ErrorCode::ok)
    {
        NX_WARNING(this, lm("Error inserting cloud owner to the local DB. %1").arg(resultCode));
        result->setError(
            QnJsonRestResult::CantProcessRequest,
            QString("Failed to save %1 owner to local DB").arg(nx::network::AppInfo::cloudName()));
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
    using namespace nx::cloud::db;

    nx::cloud::db::api::ResultCode resultCode =
        m_cloudManagerGroup->authenticationNonceFetcher.initializeConnectionToCloudSync();
    if (resultCode != nx::cloud::db::api::ResultCode::ok)
    {
        NX_WARNING(this, lm("Failed to getch cloud nonce: %1")
            .arg(nx::cloud::db::api::toString(resultCode)));
        result->setError(
            QnJsonRestResult::CantProcessRequest,
            QString("Could not connect to the %1: %2")
                .arg(nx::network::AppInfo::cloudName(),
					QString::fromStdString(nx::cloud::db::api::toString(resultCode))));
        return false;
    }

    return true;
}

bool VmsCloudConnectionProcessor::saveLocalSystemIdToCloud(
    const CloudCredentialsData& data,
    QnJsonRestResult* result)
{
    api::CloudSystemData opaque;
    opaque.localSystemId = m_commonModule->globalSettings()->localSystemId();

    nx::cloud::db::api::SystemAttributesUpdate systemAttributesUpdate;
    systemAttributesUpdate.systemId = data.cloudSystemID.toStdString();
    systemAttributesUpdate.opaque = QJson::serialized(opaque).toStdString();

    auto cloudConnection = m_cloudManagerGroup->connectionManager.getCloudConnection(
        data.cloudSystemID, data.cloudAuthKey);
    nx::cloud::db::api::ResultCode cdbResultCode = nx::cloud::db::api::ResultCode::ok;
    std::tie(cdbResultCode) =
        makeSyncCall<nx::cloud::db::api::ResultCode>(
            std::bind(
                &nx::cloud::db::api::SystemManager::update,
                cloudConnection->systemManager(),
                std::move(systemAttributesUpdate),
                std::placeholders::_1));
    if (cdbResultCode != nx::cloud::db::api::ResultCode::ok)
    {
        NX_WARNING(this, lm("Received error response from cloud: %1")
            .arg(nx::cloud::db::api::toString(cdbResultCode)));
        result->setError(
            QnJsonRestResult::CantProcessRequest,
            QString("Could not connect to the %1: %2")
                .arg(nx::network::AppInfo::cloudName(),
					QString::fromStdString(nx::cloud::db::api::toString(cdbResultCode))));
        return false;
    }

    return true;
}

bool VmsCloudConnectionProcessor::rollback()
{
    m_commonModule->globalSettings()->resetCloudParams();
    if (!m_commonModule->globalSettings()->synchronizeNowSync())
    {
        NX_WARNING(this, "Error resetting failed cloud credentials in the local DB");
        return false;
    }
    return true;
}

} // namespace cloud_integration
} // namespace vms
} // namespace nx
