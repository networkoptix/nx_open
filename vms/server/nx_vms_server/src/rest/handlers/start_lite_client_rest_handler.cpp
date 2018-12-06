#include "start_lite_client_rest_handler.h"

#include <QUrl>
#include <QProcess>

#include <nx/utils/log/log.h>
#include <nx/utils/log/log_initializer.h>
#include <nx/network/http/http_types.h>
#include "media_server/serverutil.h"
#include <rest/server/rest_connection_processor.h>
#include <network/tcp_listener.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <common/common_module.h>
#include <media_server/media_server_module.h>

namespace {

static const char* const kScriptName = "start_lite_client";

} // namespace

QnStartLiteClientRestHandler::QnStartLiteClientRestHandler(QnMediaServerModule* serverModule):
    nx::vms::server::ServerModuleAware(serverModule)
{
}

QString QnStartLiteClientRestHandler::scriptFileName() const
{ 
    return serverModule()->settings().dataDir() + "/scripts/" + kScriptName;
}

bool QnStartLiteClientRestHandler::isLiteClientPresent() const
{
    return QFile::exists(scriptFileName());
}

int QnStartLiteClientRestHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParams& params,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* connectionProcessor)
{
    const bool startCamerasMode = params.contains(lit("startCamerasMode"));

    const QString fileName = scriptFileName();
    if (!QFile::exists(fileName))
    {
        result.setError(QnRestResult::InvalidParameter, lit(
            "Script '%1' is missing at the server").arg(fileName));
        return nx::network::http::StatusCode::ok;
    }

    const int port = connectionProcessor->owner()->getPort();

    QString effectiveUserName;
    if (!connectionProcessor->accessRights().isNull())
    {
        auto user = connectionProcessor->resourcePool()->getResourceById<QnUserResource>(
            connectionProcessor->accessRights().userId);
        if (!user)
        {
            NX_ASSERT(false);
            result.setError(QnRestResult::CantProcessRequest);
            return nx::network::http::StatusCode::ok;
        }
#if 0 // Currently it is decided not to pass effectiveUserName in the URL, so, keep it empty.
        effectiveUserName = user->getName();
#endif // 0
    }

    auto server = connectionProcessor->resourcePool()->getResourceById<QnMediaServerResource>(connectionProcessor->commonModule()->moduleGUID());
    if (!server)
    {
        NX_ASSERT(false);
        result.setError(QnRestResult::CantProcessRequest);
        return nx::network::http::StatusCode::ok;
    }

    const QString userName = server->getId().toString();
    const QString password = server->getAuthKey();

    const QString urlParams = effectiveUserName.isEmpty()
        ? ""
        : lit("?effectiveUserName=%1").arg(effectiveUserName);

    const QUrl url(lit("liteclient://%1:%2@127.0.0.1:%3%4")
        .arg(userName).arg(password).arg(port).arg(urlParams));

    const QnUuid videowallInstanceGuid = server->getId();

    QStringList args{
        "--url", url.toString(),
        "--videowall-instance-guid", videowallInstanceGuid.toString(),
        "--log-level", toString(nx::utils::log::kDefaultLevel)};

    if (startCamerasMode)
        args.append(QStringList{"--auto-login", "enabled"});

    NX_VERBOSE(this, lit("startLiteClient: %1 %2").arg(fileName).arg(args.join(" ")));

    if (!QProcess::startDetached(fileName, args))
    {
        qWarning() << lit("Can't start script '%1' because of system error").arg(kScriptName);
    }

    return nx::network::http::StatusCode::ok;
}
