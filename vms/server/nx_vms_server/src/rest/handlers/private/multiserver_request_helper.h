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
    for (const auto& server: participantServers(ifPartcipantPredicate, commonModule))
    {
        const auto completionFunc =
            [&outputReply, context, serverId = server->getId(), &mergeFunction](
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

                const auto updateOutputDataCallback =
                    [&reply, success, &outputReply, context, &serverId, &mergeFunction]()
                    {
                        mergeFunction(serverId, success, reply, outputReply);
                        context->requestProcessed();
                    };

                context->executeGuarded(updateOutputDataCallback);
            };

        const nx::utils::Url apiUrl = getServerApiUrl(path, server, context);
        runMultiserverDownloadRequest(commonModule->router(), apiUrl, server, completionFunc,
            context);
        context->waitForDone();
    }
}

} // namespace detail
