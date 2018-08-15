#include "install_update_rest_handler.h"
#include "private/multiserver_request_helper.h"
#include <rest/server/rest_connection_processor.h>
#include <nx/utils/system_error.h>
#include <nx/utils/log/log.h>

namespace {

bool allServersAreReadyForInstall(const QnRestConnectionProcessor* processor)
{
    auto request = QnMultiserverRequestData::fromParams<QnEmptyRequestData>(
        processor->resourcePool(),
        QnRequestParamList());

    request.isLocal = true;
    QnMultiserverRequestContext<QnEmptyRequestData> context(
        request,
        processor->owner()->getPort());

    QList<nx::update::Status> reply;
    detail::checkUpdateStatusRemotely(processor->commonModule(), "/ec2/updateStatus", &reply, &context);

    for (const auto& status: reply)
    {
        if (status.code != nx::update::Status::Code::readyToInstall)
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
    auto allServers = detail::allServers(commonModule).toList();
    std::sort(
        allServers.begin(),
        allServers.end(),
        [commonModule](const auto& server1, const auto& server2)
        {
            return commonModule->router()->routeTo(server1->getId()).distance
                > commonModule->router()->routeTo(server2->getId()).distance;
        });

    for (const auto& server: allServers)
    {
        if (server->getId() == qnServerModule->commonModule()->moduleGUID())
            continue;

        const nx::utils::Url apiUrl = detail::getServerApiUrl(path, server, context);
        runMultiserverUploadRequest(
            commonModule->router(), apiUrl, QByteArray(),
            contentType, QString(), QString(), server,
            [server, context](SystemError::ErrorCode errorCode, int httpStatusCode)
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

                context->executeGuarded([context]() { context->requestProcessed(); });
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
    const auto request = QnMultiserverRequestData::fromParams<QnEmptyRequestData>(
        processor->resourcePool(),
        params);

    if (!request.isLocal)
    {
        if (!allServersAreReadyForInstall(processor))
        {
            return QnFusionRestHandler::makeError(
                nx::network::http::StatusCode::ok,
                "Not all servers in the system are ready for install",
                &result, &resultContentType, Qn::JsonFormat);
        }

        QnMultiserverRequestContext<QnEmptyRequestData> context(
            request,
            processor->owner()->getPort());

        sendInstallRequest(qnServerModule->commonModule(), path, srcBodyContentType, &context);
    }

    return nx::network::http::StatusCode::ok;
}

void QnInstallUpdateRestHandler::afterExecute(
    const QString& path,
    const QnRequestParamList& params,
    const QByteArray& body,
    const QnRestConnectionProcessor* owner)
{
    auto result = QJson::deserialized<QnRestResult>(body);
    if (result.error != QnRestResult::NoError)
        return;

    qnServerModule->updateManager()->install();
}
