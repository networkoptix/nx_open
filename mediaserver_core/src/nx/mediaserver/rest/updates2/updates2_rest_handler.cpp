#include "updates2_rest_handler.h"
#include <rest/server/rest_connection_processor.h>
#include <nx/utils/log/log.h>
#include <nx/network/http/http_client.h>
#include <media_server/media_server_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <nx/mediaserver/updates2/updates2_manager.h>


namespace nx {
namespace mediaserver {
namespace rest {
namespace updates2 {

namespace {

static const QString kUpdates2StatusPath = kUpdates2Path + "/status";
static const QString kUpdates2StatusAllPath = kUpdates2StatusPath + "/all";

JsonRestResponse createStatusAllResponse(const QnRestConnectionProcessor* owner)
{
    nx_http::HttpClient httpClient;
    const auto resourcePool = owner->commonModule()->resourcePool();
    QList<api::Updates2StatusData> updates2StatusDataList;
    for (const auto& server: resourcePool->getAllServers(Qn::ResourceStatus::Online))
    {
        const auto requestUrl = utils::Url(server->getApiUrl().toString() + kUpdates2StatusPath);
        const bool getResult = httpClient.doGet(requestUrl);
        boost::optional<QByteArray> body;
        api::Updates2StatusData updates2StatusData;
        if (getResult && ((body = httpClient.fetchEntireMessageBody())))
            QJson::deserialize(*body, &updates2StatusData);
        updates2StatusDataList.append(updates2StatusData);
    }

    for (const auto& server: resourcePool->getAllServers(Qn::ResourceStatus::Offline))
    {
        updates2StatusDataList.append(
            api::Updates2StatusData(
                server->getId(),
                api::Updates2StatusData::StatusCode::error,
                "Server is offline"));
    }

    JsonRestResponse result(nx_http::StatusCode::ok);
    result.json.setReply(updates2StatusDataList);
    return result;
}

JsonRestResponse createResponse()
{
    JsonRestResponse result(nx_http::StatusCode::ok);
    result.json.setReply(qnServerModule->updates2Manager()->status());
    return result;
}

} // namespace

JsonRestResponse Updates2RestHandler::executeGet(const JsonRestRequest& request)
{
    if (request.path.endsWith(kUpdates2StatusPath))
        return createResponse();

    if (request.path.endsWith(kUpdates2StatusAllPath))
        return createStatusAllResponse(request.owner);

    JsonRestResponse response(nx_http::StatusCode::notFound);
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
    return JsonRestResponse();
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
