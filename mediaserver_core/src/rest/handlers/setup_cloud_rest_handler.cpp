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
    _Fields,
    (optional, true));

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

    QnSaveCloudSystemCredentialsHandler subHandler;
    int httpResult = subHandler.execute(data, result);
    if (result.error != QnJsonRestResult::NoError)
        return httpResult;
        changeSystemName(newSystemName, 0, 0, userId);
    changeSystemName(newSystemName, 0, 0, true);
    qnGlobalSettings->setNewSystem(false);
    if (qnGlobalSettings->synchronizeNowSync())
        qnCommon->updateModuleInformation();

    const auto& settings = QnGlobalSettings::instance()->allSettings();
    for (QnAbstractResourcePropertyAdaptor* setting : settings)
    {
        auto paramIter = data.systemSettings.find(setting->key());
        if (paramIter != data.systemSettings.end())
            setting->setValue(paramIter.value());
    }

    return httpResult;
}
