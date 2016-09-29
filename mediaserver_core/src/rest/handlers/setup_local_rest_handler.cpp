#include "setup_local_rest_handler.h"

#include <api/global_settings.h>

#include <common/common_module.h>

#include <nx/network/http/httptypes.h>
#include <media_server/serverutil.h>

#include <nx/fusion/model_functions.h>
#include "rest/server/rest_connection_processor.h"
#include <api/resource_property_adaptor.h>
#include <rest/helpers/permissions_helper.h>
#include <core/resource_management/resource_pool.h>
#include "system_settings_handler.h"

namespace
{
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
    QHash<QString, QString> systemSettings;
};

#define SetupLocalSystemData_Fields PasswordData_Fields (systemName)(systemSettings)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (SetupLocalSystemData),
    (json),
    _Fields)

int QnSetupLocalSystemRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor* owner)
{
    Q_UNUSED(path);
    return execute(std::move(SetupLocalSystemData(params)), owner, result);
}

int QnSetupLocalSystemRestHandler::executePost(
    const QString &path,
    const QnRequestParams&,
    const QByteArray &body,
    QnJsonRestResult &result,
    const QnRestConnectionProcessor* owner)
{
    Q_UNUSED(path);
    SetupLocalSystemData data = QJson::deserialized<SetupLocalSystemData>(body);
    return execute(std::move(data), owner, result);
}

int QnSetupLocalSystemRestHandler::execute(
    SetupLocalSystemData data,
    const QnRestConnectionProcessor* owner,
    QnJsonRestResult &result)
{
    if (QnPermissionsHelper::isSafeMode())
        return QnPermissionsHelper::safeModeError(result);
    if (!QnPermissionsHelper::hasOwnerPermissions(owner->accessRights()))
        return QnPermissionsHelper::notOwnerError(result);

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

    if (data.systemName.isEmpty())
    {
        result.setError(QnJsonRestResult::MissingParameter, lit("Parameter 'systemName' must be provided."));
        return nx_http::StatusCode::ok;
    }

    const auto systemNameBak = qnGlobalSettings->systemName();

    qnGlobalSettings->resetCloudParams();
    qnGlobalSettings->setNewSystem(false);
    qnGlobalSettings->setSystemName(data.systemName);

    if (!qnGlobalSettings->synchronizeNowSync())
    {
        //changing system name back
        qnGlobalSettings->setSystemName(systemNameBak);
        result.setError(
            QnJsonRestResult::CantProcessRequest,
            lit("Internal server error."));
        return nx_http::StatusCode::ok;
    }

    if (!updateUserCredentials(data, QnOptionalBool(true), qnResPool->getAdministrator(), &errStr))
    {
        //changing system name back
        qnGlobalSettings->setSystemName(systemNameBak);
        result.setError(QnJsonRestResult::CantProcessRequest, errStr);
        return nx_http::StatusCode::ok;
    }

    QnSystemSettingsHandler subHandler;
    subHandler.executeGet(QString(), data.systemSettings, result, owner);

    return nx_http::StatusCode::ok;
}
