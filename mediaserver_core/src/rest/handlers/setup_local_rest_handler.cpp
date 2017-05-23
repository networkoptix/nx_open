#include "setup_local_rest_handler.h"

#include <api/global_settings.h>

#include <common/common_module.h>

#include <nx/fusion/model_functions.h>
#include <nx/network/http/http_types.h>

#include <api/model/setup_local_system_data.h>
#include <api/resource_property_adaptor.h>
#include <core/resource_management/resource_pool.h>
#include <media_server/serverutil.h>

#include "rest/server/rest_connection_processor.h"
#include "rest/helpers/permissions_helper.h"
#include "system_settings_handler.h"

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
    if (QnPermissionsHelper::isSafeMode(owner->commonModule()))
        return QnPermissionsHelper::safeModeError(result);
    if (!QnPermissionsHelper::hasOwnerPermissions(owner->resourcePool(), owner->accessRights()))
        return QnPermissionsHelper::notOwnerError(result);

    if (!owner->globalSettings()->isNewSystem())
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

    const auto systemNameBak = owner->globalSettings()->systemName();

    owner->globalSettings()->resetCloudParams();
    owner->globalSettings()->setSystemName(data.systemName);
    owner->globalSettings()->setLocalSystemId(QnUuid::createUuid());

    if (!owner->globalSettings()->synchronizeNowSync())
    {
        //changing system name back
        owner->globalSettings()->setSystemName(systemNameBak);
        owner->globalSettings()->setLocalSystemId(QnUuid()); //< revert
        result.setError(
            QnJsonRestResult::CantProcessRequest,
            lit("Internal server error."));
        return nx_http::StatusCode::ok;
    }

    if (!updateUserCredentials(
        owner->commonModule()->ec2Connection(),
        data,
        QnOptionalBool(true),
        owner->resourcePool()->getAdministrator(), &errStr))
    {
        //changing system name back
        owner->globalSettings()->setSystemName(systemNameBak);
        result.setError(QnJsonRestResult::CantProcessRequest, errStr);
        return nx_http::StatusCode::ok;
    }

    QnSystemSettingsHandler settingsHandler;
    if (!settingsHandler.updateSettings(
        owner->commonModule(),
        data.systemSettings,
        result,
        Qn::kSystemAccess,
        owner->authSession()))
    {
        qWarning() << "failed to write system settings";
    }

    return nx_http::StatusCode::ok;
}
