#include "setup_cloud_rest_handler.h"

#include <nx/network/http/httptypes.h>
#include "media_server/serverutil.h"
#include "save_cloud_system_credentials.h"

int QnSetupCloudSystemRestHandler::executeGet(const QString&, const QnRequestParams & params, QnJsonRestResult &result, const QnRestConnectionProcessor* owner)
{
    nx::SystemName systemName;
    systemName.loadFromConfig();
    if (!systemName.isDefault())
    {
        result.setError(QnJsonRestResult::Forbidden, lit("This method is allowed at initial state only. Use 'api/detachFromSystem' method first."));
        return nx_http::StatusCode::ok;
    }

    if (!changeSystemName(params.value(lit("systemName")), 0, 0))
    {
        result.setError(QnRestResult::CantProcessRequest, lit("Internal server error. Can't change system name."));
        return nx_http::StatusCode::internalServerError;
    }

    QnSaveCloudSystemCredentialsHandler subHundler;
    return subHundler.executeGet(QString(), params, result, owner);
}
