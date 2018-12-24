#include "http_server.h"

#include <nx/fusion/model_functions.h>
#include <nx/network/http/buffer_source.h>
#include <nx/network/url/url_parse_helper.h>

#include "api/peer_info.h"
#include "api/request_path.h"

namespace nx::clusterdb::engine {

HttpServer::HttpServer(const QnUuid& peerId):
    m_peerId(peerId)
{
}

void HttpServer::registerHandlers(
    const std::string& pathPrefix,
    nx::network::http::server::rest::MessageDispatcher* dispatcher)
{
    dispatcher->registerRequestProcessorFunc(
        nx::network::http::Method::get,
        nx::network::url::joinPath(pathPrefix, api::kInfo),
        [this](auto&&... args) { processInfoRequest(std::move(args)...); });
}

void HttpServer::processInfoRequest(
    nx::network::http::RequestContext /*requestContext*/,
    nx::network::http::RequestProcessedHandler completionHandler)
{
    api::PeerInfo peerInfo;
    peerInfo.id = m_peerId.toSimpleString().toStdString();

    network::http::RequestResult result(network::http::StatusCode::ok);
    result.dataSource = std::make_unique<network::http::BufferSource>(
        "application/json",
        QJson::serialized(peerInfo));

    completionHandler(std::move(result));
}

} // namespace nx::clusterdb::engine
