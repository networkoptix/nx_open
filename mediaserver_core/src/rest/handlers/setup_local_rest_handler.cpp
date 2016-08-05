#include "setup_local_rest_handler.h"

#include <api/global_settings.h>

#include <common/common_module.h>

#include <nx/network/http/httptypes.h>
#include <media_server/serverutil.h>

#include <nx/fusion/model_functions.h>
#include "rest/server/rest_connection_processor.h"
#include <api/resource_property_adaptor.h>
#include <rest/helpers/permissions_helper.h>

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
    QMap<QString, QString> systemSettings;
};

#define SetupLocalSystemData_Fields PasswordData_Fields (systemName)(systemSettings)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (SetupLocalSystemData),
    (json),
    _Fields)

int QnSetupLocalSystemRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor* owner)
{
    Q_UNUSED(path);
    return execute(std::move(SetupLocalSystemData(params)), owner->authUserId(), result);
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
    return execute(std::move(data), owner->authUserId(), result);
}

int QnSetupLocalSystemRestHandler::execute(SetupLocalSystemData data, const QnUuid &userId, QnJsonRestResult &result)
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

    const auto systemNameBak = qnCommon->localSystemName();
    if (!changeSystemName(data.systemName, 0, 0, true, Qn::UserAccessData(userId)))
    {
        result.setError(QnRestResult::CantProcessRequest, lit("Internal server error. Can't change system name."));
        return nx_http::StatusCode::ok;
    }

    qnGlobalSettings->resetCloudParams();
    qnGlobalSettings->setNewSystem(false);
    if (!qnGlobalSettings->synchronizeNowSync())
    {
        //changing system name back
        changeSystemName(systemNameBak, 0, 0, true, Qn::UserAccessData(userId));
        result.setError(
            QnJsonRestResult::CantProcessRequest,
            lit("Internal server error."));
        return nx_http::StatusCode::ok;
    }
    qnCommon->updateModuleInformation();

    if (data.systemName.isEmpty())
    {
        //changing system name back
        changeSystemName(systemNameBak, 0, 0, true, Qn::UserAccessData(userId));
        result.setError(QnJsonRestResult::MissingParameter, lit("Parameter 'systemName' must be provided."));
        return nx_http::StatusCode::ok;
    }

    QString errString;
    if (!updateAdminUser(data, QnOptionalBool(true), userId, &errString))
    {
        //changing system name back
        changeSystemName(systemNameBak, 0, 0, true, Qn::UserAccessData(userId));
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

    return nx_http::StatusCode::ok;
}
