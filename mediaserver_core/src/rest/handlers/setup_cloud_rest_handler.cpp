#include "setup_cloud_rest_handler.h"

#include <api/global_settings.h>

#include <nx/network/http/http_types.h>
#include "media_server/serverutil.h"
#include "save_cloud_system_credentials.h"
#include <core/resource_management/resource_pool.h>
#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include "rest/server/rest_connection_processor.h"
#include <api/resource_property_adaptor.h>
#include <core/resource/user_resource.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <api/app_server_connection.h>
#include <rest/helpers/permissions_helper.h>
#include <nx/vms/cloud_integration/cloud_connection_manager.h>
#include "system_settings_handler.h"


QnSetupCloudSystemRestHandler::QnSetupCloudSystemRestHandler(
    nx::vms::cloud_integration::CloudManagerGroup* cloudManagerGroup)
    :
    m_cloudManagerGroup(cloudManagerGroup)
{
}

int QnSetupCloudSystemRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor* owner)
{
    Q_UNUSED(path);
    return execute(std::move(SetupCloudSystemData(params)), owner, result);
}

int QnSetupCloudSystemRestHandler::executePost(const QString &path, const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result, const QnRestConnectionProcessor* owner)
{
    QN_UNUSED(path, params);
    const SetupCloudSystemData data = QJson::deserialized<SetupCloudSystemData>(body);
    return execute(std::move(data), owner, result);
}

int QnSetupCloudSystemRestHandler::execute(
    SetupCloudSystemData data,
    const QnRestConnectionProcessor* owner,
    QnJsonRestResult &result)
{
    if (QnPermissionsHelper::isSafeMode(owner->commonModule()))
        return QnPermissionsHelper::safeModeError(result);
    if (!QnPermissionsHelper::hasOwnerPermissions(owner->resourcePool(), owner->accessRights()))
        return QnPermissionsHelper::notOwnerError(result);


    if (!owner->globalSettings()->isNewSystem())
    {
        result.setError(QnJsonRestResult::Forbidden, lit("This method is allowed at initial state only. Use 'api/detachFromSystem' method first."));
        return nx_http::StatusCode::ok;
    }

    QString newSystemName = data.systemName;
    if (newSystemName.isEmpty())
    {
        result.setError(QnJsonRestResult::MissingParameter, lit("Parameter 'systemName' must be provided."));
        return nx_http::StatusCode::ok;
    }

    if (!owner->resourcePool()->getResourceById<QnMediaServerResource>(owner->commonModule()->moduleGUID()))
    {
        result.setError(QnJsonRestResult::CantProcessRequest, lit("Internal server error."));
        return nx_http::StatusCode::ok;
    }

    const auto systemNameBak = owner->globalSettings()->systemName();

    owner->globalSettings()->setSystemName(data.systemName);
    owner->globalSettings()->setLocalSystemId(QnUuid::createUuid());
    if (!owner->globalSettings()->synchronizeNowSync())
    {
        //changing system name back
        owner->globalSettings()->setSystemName(systemNameBak);
        owner->globalSettings()->setLocalSystemId(QnUuid()); //< revert
        result.setError(
            QnJsonRestResult::CantProcessRequest,
            lit("Internal server error."));
        return nx_http::StatusCode::ok;
    }

    QnSaveCloudSystemCredentialsHandler subHandler(m_cloudManagerGroup);
    int httpResult = subHandler.execute(data, result, owner);
    if (result.error != QnJsonRestResult::NoError)
    {
        //changing system name back
        owner->globalSettings()->setSystemName(systemNameBak);
        owner->globalSettings()->setLocalSystemId(QnUuid()); //< revert
        return httpResult;
    }
    owner->globalSettings()->synchronizeNowSync();


    QString errStr;
    PasswordData adminPasswordData;
    // reset admin password to random value to prevent NX1 root login via default password
    adminPasswordData.password = QnUuid::createUuid().toString();
    if (!updateUserCredentials(
        owner->commonModule()->ec2Connection(),
        adminPasswordData,
        QnOptionalBool(false),
        owner->resourcePool()->getAdministrator(),
        &errStr))
    {
        result.setError(QnJsonRestResult::CantProcessRequest, errStr);
        return nx_http::StatusCode::ok;
    }

    QnSystemSettingsHandler settingsHandler;
    if (!settingsHandler.updateSettings(
        owner->commonModule(),
        data.systemSettings,
        result,
        Qn::kSystemAccess,
        owner->authSession()))
    {
        qWarning() << "failed to write system settings";
    }

    return httpResult;
}
