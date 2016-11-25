#include "start_lite_client_rest_handler.h"

#include <QUrl>
#include <QProcess>

#include <nx/utils/log/log.h>
#include <nx/network/http/httptypes.h>
#include "media_server/serverutil.h"
#include <rest/server/rest_connection_processor.h>
#include <network/tcp_listener.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <common/common_module.h>

namespace {

static const char* const kScriptName = "start_lite_client";

} // namespace

int QnStartLiteClientRestHandler::executeGet(
    const QString& path,
    const QnRequestParams& params,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* connectionProcessor)
{
    Q_UNUSED(path);
    Q_UNUSED(params);
    Q_UNUSED(connectionProcessor);

    QString fileName = getDataDirectory() + "/scripts/" + kScriptName;
    if (!QFile::exists(fileName))
    {
        result.setError(QnRestResult::InvalidParameter, lit(
            "Script '%1' is missing at the server").arg(fileName));
        return nx_http::StatusCode::ok;
    }

    const int port = connectionProcessor->owner()->getPort();

    auto server = qnResPool->getResourceById<QnMediaServerResource>(qnCommon->moduleGUID());
    if (!server)
    {
        NX_ASSERT(false);
        result.setError(QnRestResult::CantProcessRequest);
        return nx_http::StatusCode::ok;
    }

    const QString userName = server->getId().toString();
    const QString password = server->getAuthKey();

    QString urlStr = lit("liteclient://%1:%2@127.0.0.1:%3").arg(userName).arg(password).arg(port);
    // If 'effectiveUserName' is disabled liteClient has full access rights
#ifdef USE_EFFECTIVE_USER_NAME
    QString effectiveUserName;
    if (!connectionProcessor->accessRights().isNull())
    {
        auto user = qnResPool->getResourceById<QnUserResource>(
            connectionProcessor->accessRights().userId);
        if (!user)
        {
            NX_ASSERT(false);
            result.setError(QnRestResult::CantProcessRequest);
            return nx_http::StatusCode::ok;
        }
        effectiveUserName = user->getName();
    }

    urlStr += lit("?effectiveUserName=%1").arg(effectiveUserName);
#endif
    const QUrl url(urlStr);

    const QnUuid videowallInstanceGuid = server->getId();

    QStringList args{
        "--url", url.toString(),
        "--videowall-instance-guid", videowallInstanceGuid.toString()};

    NX_LOG(lit("startLiteClient: %1 %2").arg(fileName).arg(args.join(" ")), cl_logDEBUG2);

    if (!QProcess::startDetached(fileName, args))
    {
        qWarning() << lit("Can't start script '%1' because of system error").arg(kScriptName);
    }

    return nx_http::StatusCode::ok;
}
