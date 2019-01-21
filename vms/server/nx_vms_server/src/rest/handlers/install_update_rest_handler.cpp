#include "install_update_rest_handler.h"
#include "private/multiserver_request_helper.h"
#include "private/multiserver_update_request_helpers.h"
#include <rest/server/rest_connection_processor.h>
#include <nx/utils/system_error.h>
#include <nx/utils/log/log.h>

namespace {

bool allParticipantsAreReadyForInstall(
    const detail::IfParticipantPredicate& ifParticipantPredicate,
    const QnRestConnectionProcessor* processor)
{
    auto request = QnMultiserverRequestData::fromParams<QnEmptyRequestData>(
        processor->resourcePool(), QnRequestParamList());
    request.isLocal = true;

    QnMultiserverRequestContext<QnEmptyRequestData> context(request,
        processor->owner()->getPort());

    QList<nx::update::Status> reply;
    detail::checkUpdateStatusRemotely(ifParticipantPredicate,processor->commonModule(),
        "/ec2/updateStatus", &reply, &context);

    return std::all_of(reply.cbegin(), reply.cend(),
        [](const auto& status)
        {
            return status.code == nx::update::Status::Code::readyToInstall
                || status.code == nx::update::Status::Code::latestUpdateInstalled;
        });
}

void sendInstallRequest(
    const detail::IfParticipantPredicate& ifParticipantPredicate,
    QnCommonModule* commonModule,
    const QString& path,
    const QByteArray& contentType,
    QnMultiserverRequestContext<QnEmptyRequestData>* context)
{
    auto allServers = detail::participantServers(ifParticipantPredicate, commonModule).toList();
    std::sort(allServers.begin(), allServers.end(),
        [commonModule](const auto& server1, const auto& server2)
        {
            return commonModule->router()->routeTo(server1->getId()).distance
                > commonModule->router()->routeTo(server2->getId()).distance;
        });

    for (const auto& server: allServers)
    {
        if (server->getId() == commonModule->moduleGUID())
            continue;

        runMultiserverUploadRequest(commonModule->router(),
            detail::getServerApiUrl(path, server, context),
            QByteArray(), contentType, QString(), QString(), server,
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

QnInstallUpdateRestHandler::QnInstallUpdateRestHandler(QnMediaServerModule* serverModule,
    nx::utils::MoveOnlyFunc<void()> onTriggeredCallback)
    :
    nx::vms::server::ServerModuleAware(serverModule),
    m_onTriggeredCallback(std::move(onTriggeredCallback))
{
}

int QnInstallUpdateRestHandler::executePost(
    const QString& path,
    const QnRequestParamList& params,
    const QByteArray& /*body*/,
    const QByteArray& srcBodyContentType,
    QByteArray& result,
    QByteArray& resultContentType,
    const QnRestConnectionProcessor* processor)
{
    const auto request = QnMultiserverRequestData::fromParams<QnEmptyRequestData>(
        processor->resourcePool(),
        params);

    m_onTriggeredCallback();

    if (!params.contains("peers"))
    {
        return QnFusionRestHandler::makeError(nx::network::http::StatusCode::ok,
            "Missing required parameter 'peers'", &result, &resultContentType, Qn::JsonFormat,
            request.extraFormatting, QnRestResult::MissingParameter);
    }

    QList<QnUuid> participants;
    if (!params.value("peers").isEmpty())
    {
        for (const auto& idString: params.value("peers").split(','))
            participants.append(QnUuid::fromStringSafe(idString));
    }

    if (!serverModule()->updateManager()->setParticipants(participants))
    {
        return QnFusionRestHandler::makeError(nx::network::http::StatusCode::ok,
            "Failed to set update participants list. Update information might not be valid",
            &result, &resultContentType, Qn::JsonFormat, request.extraFormatting,
            QnRestResult::InternalServerError);
    }

    if (!request.isLocal)
    {
        if (!serverModule()->updateManager()->updateLastInstallationRequestTime())
        {
            return QnFusionRestHandler::makeError(nx::network::http::StatusCode::ok,
                "Failed to set last installation request time. Update information might not be valid",
                &result, &resultContentType, Qn::JsonFormat, request.extraFormatting,
                QnRestResult::InternalServerError);
        }

        const auto ifParticipantPredicate = detail::makeIfParticipantPredicate(
            serverModule()->updateManager());

        if (!ifParticipantPredicate)
        {
            return QnFusionRestHandler::makeError(nx::network::http::StatusCode::ok,
                "Failed to determine update participants. Update information might not be valid",
                &result, &resultContentType, Qn::JsonFormat, request.extraFormatting,
                QnRestResult::InternalServerError);
        }

        if (!allParticipantsAreReadyForInstall(ifParticipantPredicate, processor))
        {
            return QnFusionRestHandler::makeError(nx::network::http::StatusCode::ok,
                "Not all servers in the system are ready for install",
                &result, &resultContentType, Qn::JsonFormat);
        }

        QnMultiserverRequestContext<QnEmptyRequestData> context(request,
            processor->owner()->getPort());

        sendInstallRequest(ifParticipantPredicate, serverModule()->commonModule(), path,
            srcBodyContentType, &context);
    }

    return nx::network::http::StatusCode::ok;
}

void QnInstallUpdateRestHandler::afterExecute(
    const QString& /*path*/,
    const QnRequestParamList& /*params*/,
    const QByteArray& body,
    const QnRestConnectionProcessor* owner)
{
    auto result = QJson::deserialized<QnRestResult>(body);
    if (result.error != QnRestResult::NoError)
        return;

    serverModule()->updateManager()->install(owner->authSession());
}
