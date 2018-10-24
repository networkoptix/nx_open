#include "mediator_api_client.h"

#include <nx/fusion/serialization/json.h>
#include <nx/network/http/http_client.h>
#include <nx/network/http/rest/http_rest_client.h>

#include "mediator_api_http_paths.h"

namespace nx {
namespace hpm {
namespace api {

Client::Client(const nx::utils::Url& baseMediatorApiUrl):
    base_type(baseMediatorApiUrl)
{
}

Client::~Client()
{
    pleaseStopSync();
}

void Client::getListeningPeers(
    nx::utils::MoveOnlyFunc<void(ResultCode, ListeningPeers)> completionHandler)
{
    base_type::template makeAsyncCall<ListeningPeers>(
        kStatisticsListeningPeersPath,
        std::move(completionHandler));
}

std::tuple<Client::ResultCode, ListeningPeers> Client::getListeningPeers()
{
    return base_type::template makeSyncCall<ListeningPeers>(
        kStatisticsListeningPeersPath);
}

void Client::initiateConnection(
    const ConnectRequest& request,
    nx::utils::MoveOnlyFunc<void(ResultCode, ConnectResponse)> completionHandler)
{
    base_type::template makeAsyncCall<ConnectResponse>(
        nx::network::http::rest::substituteParameters(
            kServerSessionsPath, {request.destinationHostName.toStdString()}),
        std::move(completionHandler),
        request);
}

} // namespace api
} // namespace hpm
} // namespace nx
