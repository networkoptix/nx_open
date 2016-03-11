#include "detach_rest_handler.h"

#include <nx/network/http/httptypes.h>
#include "media_server/serverutil.h"

namespace {
    static const QString kDefaultAdminPassword = "admin";
}

int QnDetachFromSystemRestHandler::executeGet(const QString &, const QnRequestParams & params, QnJsonRestResult &result, const QnRestConnectionProcessor*)
{
    PasswordData passwordData(params);
    if (!passwordData.hasPassword())
        passwordData.password = kDefaultAdminPassword;
    
    QString errStr;
    if (!validatePasswordData(passwordData, &errStr))
    {
        result.setError(QnJsonRestResult::CantProcessRequest, errStr);
        return nx_http::StatusCode::ok;
    }
    if (!changeAdminPassword(passwordData)) {
        result.setError(QnJsonRestResult::CantProcessRequest, lit("Internal server error. Can't change password."));
        return nx_http::StatusCode::ok;
    }

    nx::SystemName systemName;
    systemName.resetToDefault();
    if (!changeSystemName(systemName, 0, 0))
    {
        result.setError(QnRestResult::CantProcessRequest, lit("Internal server error.  Can't change system name."));
        return nx_http::StatusCode::internalServerError;
    }

    return nx_http::StatusCode::ok;
}
