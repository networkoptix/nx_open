#include "install_update_rest_handler.h"
#include "private/multiserver_request_helper.h"
#include "private/multiserver_update_request_helpers.h"
#include <rest/server/rest_connection_processor.h>
#include <rest/helpers/permissions_helper.h>
#include <nx/utils/system_error.h>
#include <nx/utils/log/log.h>
#include <utils/common/synctime.h>

using namespace nx::vms::server;

namespace {

struct Exception
{
    Exception(
        const QString& message,
        QnRestResult::Error restError = QnRestResult::CantProcessRequest)
        :
        message(message),
        restError(restError)
    {}

    QString message;
    QnRestResult::Error restError;
};

void makeSureParticipantsAreReadyForInstall(
    const detail::IfParticipantPredicate& ifParticipantPredicate,
    const QnRestConnectionProcessor* processor,
    QnMediaServerModule* serverModule)
{
    auto request = QnMultiserverRequestData::fromParams<QnEmptyRequestData>(
        processor->resourcePool(),
        QnRequestParamList());

    request.isLocal = true;
    QnMultiserverRequestContext<QnEmptyRequestData> context(
        request,
        processor->owner()->getPort());

    QList<nx::update::Status> reply;
    detail::checkUpdateStatusRemotely(
        ifParticipantPredicate,
        serverModule,
        "/ec2/retryUpdate", //< This will also re-check free space available for installation.
        &reply,
        &context);

    if (reply.isEmpty()
        || std::any_of(
                reply.cbegin(),
                reply.cend(),
                [](const auto& status)
                {
                    return status.code != nx::update::Status::Code::readyToInstall
                        && status.code != nx::update::Status::Code::latestUpdateInstalled;
               }))
    {
        throw Exception("Not every participant is ready for update installation");
    }
}

void sendInstallRequest(
    const detail::IfParticipantPredicate& ifParticipantPredicate,
    QnCommonModule* commonModule,
    const QString& path,
    const QByteArray& contentType,
    QnMultiserverRequestContext<QnEmptyRequestData>* context)
{
    auto allServers = detail::participantServers(ifParticipantPredicate, commonModule).toList();
    std::sort(
        allServers.begin(), allServers.end(),
        [commonModule](const auto& server1, const auto& server2)
        {
            return commonModule->router()->routeTo(server1->getId()).distance
                > commonModule->router()->routeTo(server2->getId()).distance;
        });

    for (const auto& server: allServers)
    {
        if (server->getId() == commonModule->moduleGUID())
            continue;

        runMultiserverUploadRequest(
            commonModule->router(),
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
    }

    context->waitForDone();
}

static QList<QnUuid> peersFromParams(const QnRequestParamList& params)
{
    if (!params.contains("peers"))
        throw Exception("Missing required parameter \"peers\"", QnRestResult::MissingParameter);

    QList<QnUuid> participants;
    if (!params.value("peers").isEmpty())
    {
        for (const auto& idString: params.value("peers").split(','))
            participants.append(QnUuid::fromStringSafe(idString));
    }

    return participants;
}

static int makeSuccessfulResponse(QByteArray& result, QByteArray& resultContentType)
{
    QnRestResult restResult;
    restResult.setError(QnRestResult::Error::NoError);
    QnFusionRestHandlerDetail::serialize(
        restResult,
        result,
        resultContentType,
        Qn::JsonFormat);

    return nx::network::http::StatusCode::ok;
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
        processor->resourcePool(), params);

    const auto accessRights = processor->accessRights();
    if (!QnPermissionsHelper::hasOwnerPermissions(serverModule()->resourcePool(), accessRights))
        return nx::network::http::StatusCode::forbidden;

    m_onTriggeredCallback();

    if (request.isLocal)
        return nx::network::http::StatusCode::ok;

    try
    {
        const auto peers = peersFromParams(params);
        const auto ifParticipantPredicate = detail::makeIfParticipantPredicate(
            serverModule()->updateManager(), peers);

        makeSureParticipantsAreReadyForInstall(ifParticipantPredicate, processor, serverModule());
        QnMultiserverRequestContext<QnEmptyRequestData> context(
            request,
            processor->owner()->getPort());

        if (!serverModule()->updateManager()->startUpdateInstallation(peers))
            throw Exception("Cannot start update installation");

        sendInstallRequest(
            ifParticipantPredicate,
            serverModule()->commonModule(),
            path,
            srcBodyContentType,
            &context);

        return makeSuccessfulResponse(result, resultContentType);
    }
    catch (const Exception& e)
    {
        return QnFusionRestHandler::makeError(
            nx::network::http::StatusCode::ok,
            e.message,
            &result,
            &resultContentType,
            Qn::JsonFormat,
            request.extraFormatting,
            e.restError);
    }
    catch (...)
    {
        return QnFusionRestHandler::makeError(
            nx::network::http::StatusCode::ok,
            "Internal server error",
            &result,
            &resultContentType,
            Qn::JsonFormat,
            request.extraFormatting,
            QnRestResult::Error::InternalServerError);
    }
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
