#include "setup_cloud_rest_handler.h"

#include <api/global_settings.h>

#include <nx/network/http/httptypes.h>
#include "media_server/serverutil.h"
#include "save_cloud_system_credentials.h"
#include <api/model/cloud_credentials_data.h>
#include <core/resource_management/resource_pool.h>
#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include "rest/server/rest_connection_processor.h"
#include <api/resource_property_adaptor.h>
#include <core/resource/user_resource.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <api/app_server_connection.h>
#include <rest/helpers/permissions_helper.h>
#include <cloud/cloud_connection_manager.h>


namespace
{
    static const QString kSystemNameParamName(QLatin1String("systemName"));
}

struct SetupRemoveSystemData : public CloudCredentialsData
{
    SetupRemoveSystemData() : CloudCredentialsData() {}

    SetupRemoveSystemData(const QnRequestParams& params) :
        CloudCredentialsData(params),
        systemName(params.value(kSystemNameParamName))
    {
    }

    QString systemName;
    QMap<QString, QString> systemSettings;
};

#define SetupRemoveSystemData_Fields CloudCredentialsData_Fields (systemName)(systemSettings)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (SetupRemoveSystemData),
    (json),
    _Fields)

QnSetupCloudSystemRestHandler::QnSetupCloudSystemRestHandler(
    const CloudConnectionManager& cloudConnectionManager)
:
    m_cloudConnectionManager(cloudConnectionManager)
{
}

int QnSetupCloudSystemRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor* owner)
{
    Q_UNUSED(path);
    return execute(std::move(SetupRemoveSystemData(params)), owner->authUserId(), result);
}

int QnSetupCloudSystemRestHandler::executePost(const QString &path, const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result, const QnRestConnectionProcessor* owner)
{
    QN_UNUSED(path, params);
    const SetupRemoveSystemData data = QJson::deserialized<SetupRemoveSystemData>(body);
    return execute(std::move(data), owner->authUserId(), result);
}

int QnSetupCloudSystemRestHandler::execute(SetupRemoveSystemData data, const QnUuid &userId, QnJsonRestResult &result)
{
    if (QnPermissionsHelper::isSafeMode())
        return QnPermissionsHelper::safeModeError(result);
    if (!QnPermissionsHelper::hasOwnerPermissions(userId))
        return QnPermissionsHelper::notOwnerError(result);


    if (!qnGlobalSettings->isNewSystem())
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

    if (!qnResPool->getResourceById<QnMediaServerResource>(qnCommon->moduleGUID()))
    {
        result.setError(QnJsonRestResult::CantProcessRequest, lit("Internal server error."));
        return nx_http::StatusCode::ok;
    }

    const auto systemNameBak = qnCommon->localSystemName();

    ConfigureSystemData configSystemData;
    configSystemData.systemName = newSystemName;
    configSystemData.wholeSystem = false;

    if (!changeSystemName(configSystemData))
    {
        result.setError(QnJsonRestResult::CantProcessRequest, lit("Cannot change system name"));
        return nx_http::StatusCode::ok;
    }


    QnSaveCloudSystemCredentialsHandler subHandler(m_cloudConnectionManager);
    int httpResult = subHandler.execute(data, result);
    if (result.error != QnJsonRestResult::NoError)
    {
        //changing system name back
        configSystemData.systemName = systemNameBak;
        changeSystemName(configSystemData);
        qnGlobalSettings->setNewSystem(true); //< revert
        return httpResult;
    }
    if (qnGlobalSettings->synchronizeNowSync())
        qnCommon->updateModuleInformation();


    QString errString;
    if (!updateAdminUser(PasswordData(), QnOptionalBool(false), userId, &errString))
    {
        result.setError(QnJsonRestResult::CantProcessRequest, errString);
        return nx_http::StatusCode::ok;
    }

    const auto& settings = QnGlobalSettings::instance()->allSettings();
    for (QnAbstractResourcePropertyAdaptor* setting : settings)
    {
        auto paramIter = data.systemSettings.find(setting->key());
        if (paramIter != data.systemSettings.end())
            setting->setValue(paramIter.value());
    }

    return httpResult;
}
