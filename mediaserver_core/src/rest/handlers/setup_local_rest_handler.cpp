#include "setup_local_rest_handler.h"

#include <nx/network/http/httptypes.h>
#include "media_server/serverutil.h"

int QnSetupLocalSystemRestHandler::executeGet(const QString &, const QnRequestParams & params, QnJsonRestResult &result, const QnRestConnectionProcessor*)
{
    nx::SystemName systemName;
    systemName.loadFromConfig();
    if (!systemName.isDefault())
    {
        result.setError(QnJsonRestResult::Forbidden, lit("This method is allowed at initial state only. Use 'api/detachFromSystem' method first."));
        return nx_http::StatusCode::ok;
    }

    QString errStr;
    PasswordData passwordData(params);
    if (!validatePasswordData(passwordData, &errStr))
    {
        result.setError(QnJsonRestResult::CantProcessRequest, errStr);
        return nx_http::StatusCode::ok;
    }
    if (!passwordData.hasPassword())
    {
        result.setError(QnJsonRestResult::MissingParameter, "Password or password digest MUST be provided");
        return nx_http::StatusCode::ok;
    }

    QString newSystemName = params.value(lit("systemName"));
    if (newSystemName.isEmpty())
    {
        result.setError(QnJsonRestResult::MissingParameter, lit("Parameter 'systemName' must be provided."));
        return nx_http::StatusCode::ok;
    }

    if (!changeAdminPassword(passwordData)) {
        result.setError(QnJsonRestResult::CantProcessRequest, lit("Internal server error. Can't change password."));
        return nx_http::StatusCode::ok;
    }

    if (!changeSystemName(newSystemName, 0, 0))
    {
        result.setError(QnRestResult::CantProcessRequest, lit("Internal server error. Can't change system name."));
        return nx_http::StatusCode::internalServerError;
    }

    return nx_http::StatusCode::ok;
}
