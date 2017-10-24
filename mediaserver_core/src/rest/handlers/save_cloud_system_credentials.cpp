#include "save_cloud_system_credentials.h"

#include <nx/cloud/cdb/api/connection.h>

#include <nx/network/http/http_types.h>
#include <nx/utils/log/log.h>
#include <nx/utils/string.h>

#include <api/app_server_connection.h>
#include <api/model/cloud_credentials_data.h>
#include <api/global_settings.h>
#include <media_server/serverutil.h>
#include <nx/utils/sync_call.h>

#include <nx_ec/data/api_cloud_system_data.h>
#include <nx/fusion/model_functions.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <common/common_module.h>
#include <nx/vms/cloud_integration/cloud_connection_manager.h>
#include <nx/vms/cloud_integration/cloud_manager_group.h>

#include <rest/helpers/permissions_helper.h>
#include <rest/server/rest_connection_processor.h>

QnSaveCloudSystemCredentialsHandler::QnSaveCloudSystemCredentialsHandler(
    nx::vms::cloud_integration::CloudManagerGroup* cloudManagerGroup)
:
    m_cloudManagerGroup(cloudManagerGroup)
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

    nx_http::StatusCode::Value statusCode = nx_http::StatusCode::ok;
    if (!authorize(owner, &result, &statusCode))
        return statusCode;

    if (!validateInputData(owner->commonModule(), data, &result))
        return nx_http::StatusCode::badRequest;

    if (!checkInternetConnection(owner->commonModule(), &result))
        return nx_http::StatusCode::badRequest;

    if (!saveCloudData(owner->commonModule(), data, &result))
        return nx_http::StatusCode::internalServerError;

    // Trying to connect to the cloud to activate system.
    // We connect to the cloud after saving properties to DB because
    // if server activates system in cloud and then save it to DB,
    // crash can result in unsynchronized unrecoverable state: there
    // is some system in cloud, but system does not know its credentials
    // and there is no way to find them out.

    if (!fetchNecessaryDataFromCloud(owner->commonModule(), data, &result))
    {
        if (rollback(owner->commonModule()))
            return nx_http::StatusCode::serviceUnavailable;
        else
            return nx_http::StatusCode::internalServerError;
    }

    result.setError(QnJsonRestResult::NoError);
    return nx_http::StatusCode::ok;
}

bool QnSaveCloudSystemCredentialsHandler::authorize(
    const QnRestConnectionProcessor* owner,
    QnJsonRestResult* result,
    nx_http::StatusCode::Value* const authorizationStatusCode)
{
    using namespace nx_http;
    if (QnPermissionsHelper::isSafeMode(owner->commonModule()))
    {
        *authorizationStatusCode = (StatusCode::Value)QnPermissionsHelper::safeModeError(*result);
        return false;
    }
    if (!QnPermissionsHelper::hasOwnerPermissions(owner->commonModule()->resourcePool(), owner->accessRights()))
    {
        *authorizationStatusCode = (StatusCode::Value)QnPermissionsHelper::notOwnerError(*result);
        return false;
    }

    return true;
}

bool QnSaveCloudSystemCredentialsHandler::validateInputData(
    QnCommonModule* commonModule,
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

    const QString cloudSystemId = commonModule->globalSettings()->cloudSystemId();
    if (!cloudSystemId.isEmpty() &&
        !commonModule->globalSettings()->cloudAuthKey().isEmpty())
    {
        NX_LOGX(lit("Attempt to bind to cloud already-bound system"), cl_logDEBUG1);
        result->setError(
            QnJsonRestResult::CantProcessRequest,
            tr("System already bound to cloud (id %1)").arg(cloudSystemId));
        return false;
    }

    return true;
}

bool QnSaveCloudSystemCredentialsHandler::checkInternetConnection(
    QnCommonModule* commonModule,
    QnJsonRestResult* result)
{
    auto server = commonModule->resourcePool()->getResourceById<QnMediaServerResource>(commonModule->moduleGUID());
    bool hasPublicIP = server && server->getServerFlags().testFlag(Qn::SF_HasPublicIP);
    if (!hasPublicIP)
    {
        result->setError(
            QnJsonRestResult::CantProcessRequest,
            tr("Server is not connected to the Internet."));
        NX_LOGX(result->errorString, cl_logWARNING);
        return false;
    }

    return true;
}

bool QnSaveCloudSystemCredentialsHandler::saveCloudData(
    QnCommonModule* commonModule,
    const CloudCredentialsData& data,
    QnJsonRestResult* result)
{
    if (!saveCloudCredentials(commonModule, data, result))
        return false;

    if (!insertCloudOwner(commonModule, data, result))
        return false;

    return true;
}

bool QnSaveCloudSystemCredentialsHandler::saveCloudCredentials(
    QnCommonModule* commonModule,
    const CloudCredentialsData& data,
    QnJsonRestResult* result)
{
    NX_LOGX(lm("Saving cloud credentials"), cl_logDEBUG1);

    commonModule->globalSettings()->setCloudSystemId(data.cloudSystemID);
    commonModule->globalSettings()->setCloudAccountName(data.cloudAccountName);
    commonModule->globalSettings()->setCloudAuthKey(data.cloudAuthKey);
    if (!commonModule->globalSettings()->synchronizeNowSync())
    {
        NX_LOGX(lit("Error saving cloud credentials to the local DB"), cl_logWARNING);
        result->setError(
            QnJsonRestResult::CantProcessRequest,
            tr("Failed to save cloud credentials to local DB"));
        return false;
    }

    return true;
}

bool QnSaveCloudSystemCredentialsHandler::insertCloudOwner(
    QnCommonModule* commonModule,
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
        commonModule->ec2Connection()
            ->getUserManager(Qn::kSystemAccess)->saveSync(userData);
    if (resultCode != ec2::ErrorCode::ok)
    {
        NX_LOGX(
            lm("Error inserting cloud owner to the local DB. %1").arg(resultCode),
            cl_logWARNING);
        result->setError(
            QnJsonRestResult::CantProcessRequest,
            tr("Failed to save cloud owner to local DB"));
        return false;
    }

    return true;
}

bool QnSaveCloudSystemCredentialsHandler::fetchNecessaryDataFromCloud(
    QnCommonModule* commonModule,
    const CloudCredentialsData& data,
    QnJsonRestResult* result)
{
    return saveLocalSystemIdToCloud(commonModule, data, result) &&
        initializeCloudRelatedManagers(data, result);
}

bool QnSaveCloudSystemCredentialsHandler::initializeCloudRelatedManagers(
    const CloudCredentialsData& /*data*/,
    QnJsonRestResult* result)
{
    using namespace nx::cdb;

    api::ResultCode resultCode =
        m_cloudManagerGroup->authenticationNonceFetcher.initializeConnectionToCloudSync();
    if (resultCode != api::ResultCode::ok)
    {
        NX_LOGX(lm("Failed to getch cloud nonce: %1")
            .arg(api::toString(resultCode)), cl_logWARNING);
        result->setError(
            QnJsonRestResult::CantProcessRequest,
            tr("Could not connect to cloud: %1")
                .arg(QString::fromStdString(api::toString(resultCode))));
        return false;
    }

    return true;
}

bool QnSaveCloudSystemCredentialsHandler::saveLocalSystemIdToCloud(
    QnCommonModule* commonModule,
    const CloudCredentialsData& data,
    QnJsonRestResult* result)
{
    using namespace nx::cdb;

    ec2::ApiCloudSystemData opaque;
    opaque.localSystemId = commonModule->globalSettings()->localSystemId();

    api::SystemAttributesUpdate systemAttributesUpdate;
    systemAttributesUpdate.systemId = data.cloudSystemID.toStdString();
    systemAttributesUpdate.opaque = QJson::serialized(opaque).toStdString();

    auto cloudConnection = m_cloudManagerGroup->connectionManager.getCloudConnection(
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
        result->setError(
            QnJsonRestResult::CantProcessRequest,
            tr("Could not connect to cloud: %1")
                .arg(QString::fromStdString(api::toString(cdbResultCode))));
        return false;
    }

    return true;
}

bool QnSaveCloudSystemCredentialsHandler::rollback(QnCommonModule* commonModule)
{
    commonModule->globalSettings()->resetCloudParams();
    if (!commonModule->globalSettings()->synchronizeNowSync())
    {
        NX_LOGX(lit("Error resetting failed cloud credentials in the local DB"), cl_logWARNING);
        return false;
    }
    return true;
}
