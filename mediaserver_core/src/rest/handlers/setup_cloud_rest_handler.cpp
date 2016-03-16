#include "setup_cloud_rest_handler.h"

#include <nx/network/http/httptypes.h>
#include "media_server/serverutil.h"
#include "save_cloud_system_credentials.h"
#include <api/model/cloud_credentials_data.h>

struct SetupRemoveSystemData : public CloudCredentialsData
{
    QString systemName;
};

#define SetupRemoveSystemData_Fields CloudCredentialsData_Fields (systemName)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (SetupRemoveSystemData),
    (json),
    _Fields,
    (optional, true));

int QnSetupCloudSystemRestHandler::executePost(const QString &path, const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result, const QnRestConnectionProcessor*)
{
    const SetupRemoveSystemData data = QJson::deserialized<SetupRemoveSystemData>(body);

    nx::SystemName systemName;
    systemName.loadFromConfig();
    if (!systemName.isDefault())
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

    if (!changeSystemName(newSystemName, 0, 0))
    {
        result.setError(QnRestResult::CantProcessRequest, lit("Internal server error. Can't change system name."));
        return nx_http::StatusCode::internalServerError;
    }

    QnSaveCloudSystemCredentialsHandler subHundler;
    return subHundler.execute(data, result);
}
