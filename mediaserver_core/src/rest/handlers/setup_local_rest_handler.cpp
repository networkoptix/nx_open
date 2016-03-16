#include "setup_local_rest_handler.h"

#include <nx/network/http/httptypes.h>
#include <media_server/serverutil.h>

namespace
{
    static const QString kDefaultAdminPassword(QLatin1String("admin"));
}

struct SetupLocalSystemData: public PasswordData
{
    QString systemName;
};

#define SetupLocalSystemData_Fields PasswordData_Fields (systemName)


QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (SetupLocalSystemData),
    (json),
    _Fields,
    (optional, true));


int QnSetupLocalSystemRestHandler::executePost(
    const QString &path,
    const QnRequestParams&,
    const QByteArray &body,
    QnJsonRestResult &result,
    const QnRestConnectionProcessor*)
{
    nx::SystemName systemName;
    systemName.loadFromConfig();
    if (!systemName.isDefault())
    {
        result.setError(QnJsonRestResult::Forbidden, lit("This method is allowed at initial state only. Use 'api/detachFromSystem' method first."));
        return nx_http::StatusCode::ok;
    }

    QString errStr;
    SetupLocalSystemData data = QJson::deserialized<SetupLocalSystemData>(body);
    if (data.oldPassword.isEmpty())
        data.oldPassword = kDefaultAdminPassword;

    if (!validatePasswordData(data, &errStr))
    {
        result.setError(QnJsonRestResult::CantProcessRequest, errStr);
        return nx_http::StatusCode::ok;
    }
    if (!data.hasPassword())
    {
        result.setError(QnJsonRestResult::MissingParameter, "Password or password digest MUST be provided");
        return nx_http::StatusCode::ok;
    }

    if (data.systemName.isEmpty())
    {
        result.setError(QnJsonRestResult::MissingParameter, lit("Parameter 'systemName' must be provided."));
        return nx_http::StatusCode::ok;
    }

    if (!changeAdminPassword(data)) {
        result.setError(QnJsonRestResult::CantProcessRequest, lit("Internal server error. Can't change password."));
        return nx_http::StatusCode::ok;
    }

    if (!changeSystemName(data.systemName, 0, 0))
    {
        result.setError(QnRestResult::CantProcessRequest, lit("Internal server error. Can't change system name."));
        return nx_http::StatusCode::internalServerError;
    }

    return nx_http::StatusCode::ok;
}
