#pragma once

#include <QtCore/QDir>

#include <rest/helpers/request_context.h>
#include <rest/helpers/request_helpers.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <network/tcp_listener.h>
#include <nx/vms/discovery/manager.h>
#include <api/global_settings.h>
#include <nx/vms/api/types/resource_types.h>
#include <media_server/media_server_module.h>
#include <nx/vms/server/settings.h>
#include <api/helpers/empty_request_data.h>
#include <nx/vms/server/server_update_manager.h>
#include <rest/server/fusion_rest_handler.h>

namespace detail {

template<typename ContextType>
nx::utils::Url getServerApiUrl(const QString& path, const QnMediaServerResourcePtr& server,
    ContextType context)
{
    nx::utils::Url result(server->getApiUrl());
    result.setPath(path);

    auto modifiedRequest = context->request();
    modifiedRequest.makeLocal(Qn::JsonFormat);
    result.setQuery(modifiedRequest.toUrlQuery());

    return result;
}

enum class ParticipationStatus
{
    participant,
    notInList,
    incompatibleVersion
};

using IfParticipantPredicate =
    nx::utils::MoveOnlyFunc<ParticipationStatus(
        const QnUuid&,
        const nx::vms::api::SoftwareVersion&)>;

QSet<QnMediaServerResourcePtr> participantServers(
    const IfParticipantPredicate& ifPartcipantPredicate,
    QnCommonModule* commonModule);

template<typename ReplyType, typename MergeFunction, typename RequestData>
void requestRemotePeers(
    QnCommonModule* commonModule,
    const QString& path,
    ReplyType& outputReply,
    QnMultiserverRequestContext<RequestData>* context,
    const MergeFunction& mergeFunction,
    const IfParticipantPredicate& ifPartcipantPredicate = nullptr)
{
    static auto kMultiRequestLogTag = nx::utils::log::Tag(QString("MultiRequest"));
    for (const auto& server: participantServers(ifPartcipantPredicate, commonModule))
    {
        NX_DEBUG(
            nx::utils::log::Tag(kMultiRequestLogTag), "Sending multiserver request %1 to %2",
            path, server->getId());

        const auto completionFunc =
            [&outputReply, context, serverId = server->getId(), &mergeFunction, &path](
                SystemError::ErrorCode /*osErrorCode*/,
                int statusCode,
                nx::network::http::BufferType body,
                nx::network::http::HttpHeaders /*headers*/)
            {
                ReplyType reply;
                bool success = false;

                const auto httpCode = static_cast<nx::network::http::StatusCode::Value>(statusCode);
                if (httpCode == nx::network::http::StatusCode::ok)
                    reply = QJson::deserialized(body, reply, &success);

                NX_DEBUG(
                    kMultiRequestLogTag,
                    "Received a response for the multiserver request %1 to %2. "
                    "Success: %3, reply size: %4", path, serverId, success, body.size());

                const auto updateOutputDataCallback =
                    [&reply, success, &outputReply, context, &serverId, &mergeFunction]()
                    {
                        mergeFunction(serverId, success, reply, outputReply);
                        context->requestProcessed();
                    };

                context->executeGuarded(updateOutputDataCallback);
            };

        runMultiserverDownloadRequest(
            commonModule->router(),
            getServerApiUrl(path, server, context),
            server,
            completionFunc,
            context);
    }

    context->waitForDone();
}

bool verifyPasswordOrSetError(
    const QnRestConnectionProcessor* owner,
    const QString& currentPassword,
    QnJsonRestResult* result);

} // namespace detail
