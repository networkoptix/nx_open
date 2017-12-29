#include "restore_state_rest_handler.h"

#include <nx/network/http/http_types.h>

#include <nx/vms/utils/vms_utils.h>

#include <rest/server/rest_connection_processor.h>
#include <media_server/serverutil.h>
#include <media_server/settings.h>
#include <server/server_globals.h>
#include <media_server_process.h>
#include <rest/helpers/permissions_helper.h>

int QnRestoreStateRestHandler::executeGet(
    const QString& path,
    const QnRequestParams & params,
    QnJsonRestResult &result,
    const QnRestConnectionProcessor* owner)
{
    Q_UNUSED(path);
    return execute(std::move(PasswordData(params)), owner, result);
}

int QnRestoreStateRestHandler::executePost(
    const QString &path,
    const QnRequestParams &params,
    const QByteArray &body,
    QnJsonRestResult &result,
    const QnRestConnectionProcessor* owner)
{
    Q_UNUSED(path);
    Q_UNUSED(params);

    PasswordData passwordData = QJson::deserialized<PasswordData>(body);
    return execute(std::move(passwordData), owner, result);
}

int QnRestoreStateRestHandler::execute(
    PasswordData passwordData,
    const QnRestConnectionProcessor* owner,
    QnJsonRestResult &result)
{
    const Qn::UserAccessData& accessRights = owner->accessRights();

    if (QnPermissionsHelper::isSafeMode(owner->commonModule()))
        return QnPermissionsHelper::safeModeError(result);
    if (!QnPermissionsHelper::hasOwnerPermissions(owner->resourcePool(), accessRights))
        return QnPermissionsHelper::notOwnerError(result);

    QString errStr;
    if (!nx::vms::utils::validatePasswordData(passwordData, &errStr))
    {
        result.setError(QnJsonRestResult::CantProcessRequest, errStr);
        return nx::network::http::StatusCode::ok;
    }

    if (QnPermissionsHelper::isSafeMode(owner->commonModule()))
        return QnPermissionsHelper::safeModeError(result);

    return nx::network::http::StatusCode::ok;
}

void QnRestoreStateRestHandler::afterExecute(
    const QString& path,
    const QnRequestParamList& params,
    const QByteArray& body,
    const QnRestConnectionProcessor* owner)
{
    Q_UNUSED(path);
    Q_UNUSED(params);
    Q_UNUSED(owner);

    QnJsonRestResult reply;
    if (QJson::deserialize(body, &reply) && reply.error == QnJsonRestResult::NoError)
    {
        qnServerModule->roSettings()->setValue(QnServer::kRemoveDbParamName, "1");
        restartServer(0);
    }
}
