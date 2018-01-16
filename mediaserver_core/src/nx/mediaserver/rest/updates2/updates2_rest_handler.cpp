#include <memory>
#include <vector>
#include "updates2_rest_handler.h"
#include <rest/server/rest_connection_processor.h>
#include <nx/utils/log/log.h>
#include <nx/network/http/http_client.h>
#include <media_server/media_server_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <nx/mediaserver/updates2/updates2_manager.h>
#include <rest/helpers/request_helpers.h>
#include <rest/helpers/request_context.h>
#include <api/helpers/empty_request_data.h>
#include <network/tcp_listener.h>
#include <nx/utils/std/future.h>
#include <nx/api/updates2/updates2_action_data.h>


namespace nx {
namespace mediaserver {
namespace rest {
namespace updates2 {

namespace {

static const QString kUpdates2StatusPath = kUpdates2Path + "/status";
static const QString kUpdates2StatusAllPath = kUpdates2StatusPath + "/all";

static JsonRestResponse createStatusAllResponse(const QnRestConnectionProcessor* owner)
{
    const auto resourcePool = owner->commonModule()->resourcePool();
    QList<api::Updates2StatusData> updates2StatusDataList;
    QnEmptyRequestData request;
    QnMultiserverRequestContext<QnEmptyRequestData> context(request, owner->owner()->getPort());
    std::vector<utils::future<void>> responseFutures;

    for (const auto& server: resourcePool->getAllServers(Qn::ResourceStatus::Online))
    {
        if (server->getId() == qnServerModule->commonModule()->moduleGUID())
            continue;

        auto requestUrl = server->getApiUrl();
        requestUrl.setPath("/" + kUpdates2StatusPath);
        NX_ASSERT(requestUrl.isValid());
        auto responsePromisePtr = std::make_shared<utils::promise<void>>();
        responseFutures.emplace_back(responsePromisePtr->get_future());

        runMultiserverDownloadRequest(
            qnServerModule->commonModule()->router(),
            requestUrl,
            server,
            [&context, &updates2StatusDataList, serverId = server->getId(), responsePromisePtr](
                SystemError::ErrorCode osErrorCode,
                int statusCode,
                network::http::BufferType msgBody) mutable
            {

                api::Updates2StatusData updates2StatusData(
                    serverId,
                    api::Updates2StatusData::StatusCode::error,
                    "Multi update request failed");

                if (osErrorCode == SystemError::noError
                    && statusCode == network::http::StatusCode::ok)
                {
                    QnJsonRestResult restResult;
                    bool deserializeResult = QJson::deserialize(msgBody, &restResult);
                    NX_ASSERT(deserializeResult);
                    if (deserializeResult && restResult.error == QnRestResult::Error::NoError)
                    {
                        deserializeResult = QJson::deserialize<api::Updates2StatusData>(
                            restResult.reply,
                            &updates2StatusData);
                        NX_ASSERT(deserializeResult);
                    }
                }

                context.executeGuarded(
                    [&context, &updates2StatusData, &updates2StatusDataList]()
                    {
                        updates2StatusDataList.append(updates2StatusData);
                        context.requestProcessed();
                    });

                responsePromisePtr->set_value();
            },
            &context);
    }

    for (const auto& server : resourcePool->getAllServers(Qn::ResourceStatus::Offline))
    {
        updates2StatusDataList.append(
            api::Updates2StatusData(
                server->getId(),
                api::Updates2StatusData::StatusCode::error,
                "Server is offline"));
    }

    updates2StatusDataList.append(qnServerModule->updates2Manager()->status());

    for (auto& future: responseFutures)
        future.wait();

    JsonRestResponse result(network::http::StatusCode::ok);
    result.json.setReply(updates2StatusDataList);
    return result;
}

JsonRestResponse createResponse()
{
    JsonRestResponse result(network::http::StatusCode::ok);
    result.json.setReply(qnServerModule->updates2Manager()->status());
    return result;
}

JsonRestResponse createActionResponse(const QByteArray& body)
{
    JsonRestResponse result(network::http::StatusCode::ok);
    api::Updates2ActionData actionData;

    if (!QJson::deserialize<api::Updates2ActionData>(body, &actionData))
    {
        result.json.setReply(
            api::Updates2StatusData(
                qnServerModule->commonModule()->moduleGUID(),
                api::Updates2StatusData::StatusCode::error,
                "Failed to deserialize update action data"));
        return result;
    }

    switch (actionData.action)
    {
        case api::Updates2ActionData::ActionCode::download:
            result.json.setReply(qnServerModule->updates2Manager()->download());
            break;
        case api::Updates2ActionData::ActionCode::install:
            break;
        case api::Updates2ActionData::ActionCode::stop:
            break;
    }

    return result;
}

} // namespace

JsonRestResponse Updates2RestHandler::executeGet(const JsonRestRequest& request)
{
    if (request.path.endsWith(kUpdates2StatusPath))
        return createResponse();

    if (request.path.endsWith(kUpdates2StatusAllPath))
        return createStatusAllResponse(request.owner);

    JsonRestResponse response(network::http::StatusCode::notFound);
    response.json.setError(QnRestResult::Error::CantProcessRequest);
    return response;
}

JsonRestResponse Updates2RestHandler::executeDelete(const JsonRestRequest& request)
{
    return JsonRestResponse();
}

JsonRestResponse Updates2RestHandler::executePost(
    const JsonRestRequest& request,
    const QByteArray& body)
{
    if (request.path == "/" + kUpdates2Path)
        return createActionResponse(body);

    JsonRestResponse response(network::http::StatusCode::notFound);
    response.json.setError(QnRestResult::Error::CantProcessRequest);
    return response;
}

JsonRestResponse Updates2RestHandler::executePut(
    const JsonRestRequest& request,
    const QByteArray& body)
{
    return JsonRestResponse();
}

} // namespace updates2
} // namespace rest
} // namespace mediaserver
} // namespace nx
