#include "setup_cloud_rest_handler.h"

#include <api/global_settings.h>

#include <nx/network/http/httptypes.h>
#include "media_server/serverutil.h"
#include "save_cloud_system_credentials.h"
#include <api/model/cloud_credentials_data.h>
#include <core/resource_management/resource_pool.h>
#include <common/common_module.h>
#include <core/resource/media_server_resource.h>

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
};

#define SetupRemoveSystemData_Fields CloudCredentialsData_Fields (systemName)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (SetupRemoveSystemData),
    (json),
    _Fields,
    (optional, true));

int QnSetupCloudSystemRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*)
{
    Q_UNUSED(path);
    return execute(std::move(SetupRemoveSystemData(params)), result);
}

int QnSetupCloudSystemRestHandler::executePost(const QString &path, const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result, const QnRestConnectionProcessor*)
{
    QN_UNUSED(path, params);
    const SetupRemoveSystemData data = QJson::deserialized<SetupRemoveSystemData>(body);
    return execute(std::move(data), result);
}

int QnSetupCloudSystemRestHandler::execute(SetupRemoveSystemData data, QnJsonRestResult &result)
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

    QnSaveCloudSystemCredentialsHandler subHundler;
    int httpResult = subHundler.execute(data, result);
    if (result.error == QnJsonRestResult::NoError)
    {
        changeSystemName(newSystemName, 0, 0);
        qnGlobalSettings->setNewSystem(false);
        qnGlobalSettings->synchronizeNow();
    }
    return httpResult;
}
