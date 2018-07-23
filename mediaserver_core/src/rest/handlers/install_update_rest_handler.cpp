#include "install_update_rest_handler.h"
#include "private/multiserver_request_helper.h"
#include <rest/server/rest_connection_processor.h>
#include <nx/utils/system_error.h>
#include <nx/utils/log/log.h>

namespace {

bool allServersAreReadyForInstall(const QnRestConnectionProcessor* processor)
{
    const auto request = QnMultiserverRequestData::fromParams<QnEmptyRequestData>(
        processor->resourcePool(),
        QnRequestParamList());

    QnMultiserverRequestContext<QnEmptyRequestData> context(
        request,
        processor->owner()->getPort());

    QList<nx::UpdateStatus> reply;
    detail::checkUpdateStatusRemotely(processor->commonModule(), "/updateStatus", &reply, &context);

    for (const auto& status: reply)
    {
        if (status.code != nx::UpdateStatus::Code::readyToInstall)
            return false;
    }

    return true;
}

void sendInstallRequest(
    QnCommonModule* commonModule,
    const QString& path,
    const QByteArray& contentType,
    QnMultiserverRequestContext<QnEmptyRequestData>* context)
{
    for (const auto& server : detail::allServers(commonModule))
    {
        const nx::utils::Url apiUrl = detail::getServerApiUrl(path, server, context);
        runMultiserverUploadRequest(
            commonModule->router(), apiUrl, QByteArray(),
            contentType, QString(), QString(), server,
            [server](SystemError::ErrorCode errorCode, int httpStatusCode)
            {
                if (errorCode != SystemError::noError)
                {
                    NX_WARNING(
                        typeid(QnInstallUpdateRestHandler),
                        lm("installUpdate request to server %1 failed").args(server->getId()));
                }

                if (httpStatusCode != nx::network::http::StatusCode::ok)
                {
                    NX_WARNING(
                        typeid(QnInstallUpdateRestHandler),
                        lm("installUpdate request to server %1 failed with http code")
                        .args(server->getId(), httpStatusCode));
                }
            },
            context);
        context->waitForDone();
    }
}

} // namespace

int QnInstallUpdateRestHandler::executePost(
    const QString& path,
    const QnRequestParamList& params,
    const QByteArray& body,
    const QByteArray& srcBodyContentType,
    QByteArray& result,
    QByteArray& resultContentType,
    const QnRestConnectionProcessor* processor)
{
    if (!allServersAreReadyForInstall(processor))
    {
        return QnFusionRestHandler::makeError(
            nx::network::http::StatusCode::ok,
            "Not all servers in the system are ready for install",
            &result, &resultContentType, Qn::JsonFormat);
    }

    const auto request = QnMultiserverRequestData::fromParams<QnEmptyRequestData>(
        processor->resourcePool(),
        params);

    QnMultiserverRequestContext<QnEmptyRequestData> context(
        request,
        processor->owner()->getPort());

    if (!request.isLocal)
        sendInstallRequest(qnServerModule->commonModule(), path, srcBodyContentType, &context);

    qnServerModule->updateManager()->install();
    return nx::network::http::StatusCode::ok;
}
