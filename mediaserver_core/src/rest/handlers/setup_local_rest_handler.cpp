#include "setup_local_rest_handler.h"

#include <api/global_settings.h>

#include <common/common_module.h>

#include <nx/network/http/httptypes.h>
#include <media_server/serverutil.h>

#include <utils/common/model_functions.h>

namespace
{
    static const QString kDefaultAdminPassword(QLatin1String("admin"));
    static const QString kSystemNameParamName(QLatin1String("systemName"));
}

struct SetupLocalSystemData: public PasswordData
{
    SetupLocalSystemData() : PasswordData() {}

    SetupLocalSystemData(const QnRequestParams& params):
        PasswordData(params),
        systemName(params.value(kSystemNameParamName))
    {
    }

    QString systemName;
};

#define SetupLocalSystemData_Fields PasswordData_Fields (systemName)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (SetupLocalSystemData),
    (json),
    _Fields,
    (optional, true));

int QnSetupLocalSystemRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*)
{
    Q_UNUSED(path);
    return execute(std::move(SetupLocalSystemData(params)), result);
}

int QnSetupLocalSystemRestHandler::executePost(
    const QString &path,
    const QnRequestParams&,
    const QByteArray &body,
    QnJsonRestResult &result,
    const QnRestConnectionProcessor*)
{
    Q_UNUSED(path);
    SetupLocalSystemData data = QJson::deserialized<SetupLocalSystemData>(body);
    return execute(std::move(data), result);
}

int QnSetupLocalSystemRestHandler::execute(SetupLocalSystemData data, QnJsonRestResult &result)
{
    if (data.oldPassword.isEmpty())
        data.oldPassword = kDefaultAdminPassword;

    if (!qnGlobalSettings->isNewSystem())
    {
        result.setError(QnJsonRestResult::Forbidden, lit("This method is allowed at initial state only. Use 'api/detachFromSystem' method first."));
        return nx_http::StatusCode::ok;
    }

    QString errStr;
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

    qnGlobalSettings->resetCloudParams();
    qnGlobalSettings->setNewSystem(false);
    if (!qnGlobalSettings->synchronizeNowSync())
    {
        result.setError(
            QnJsonRestResult::CantProcessRequest,
            lit("Internal server error."));
        return nx_http::StatusCode::ok;
    }
    qnCommon->updateModuleInformation();

    if (data.systemName.isEmpty())
    {
        result.setError(QnJsonRestResult::MissingParameter, lit("Parameter 'systemName' must be provided."));
        return nx_http::StatusCode::ok;
    }

    QString errString;
    if (!changeAdminPassword(data, &errString)) {
        result.setError(QnJsonRestResult::CantProcessRequest, errString);
        return nx_http::StatusCode::ok;
    }

    if (!changeSystemName(data.systemName, 0, 0))
    {
        result.setError(QnRestResult::CantProcessRequest, lit("Internal server error. Can't change system name."));
        return nx_http::StatusCode::internalServerError;
    }

    return nx_http::StatusCode::ok;
}
