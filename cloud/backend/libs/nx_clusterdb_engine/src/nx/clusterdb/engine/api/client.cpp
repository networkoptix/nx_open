#include "client.h"

#include <nx/network/http/rest/http_rest_client.h>

#include "request_path.h"

namespace nx::clusterdb::engine::api {

Client::Client(
    const nx::utils::Url& baseApiUrl,
    const std::string& systemId)
    :
    base_type(nx::network::url::Builder(baseApiUrl)
        .setPath(nx::network::http::rest::substituteParameters(
            baseApiUrl.path().toStdString(), {systemId}))),
    m_systemId(systemId)
{
}

Client::~Client()
{
    pleaseStopSync();
}

void Client::getInfo(
    nx::utils::MoveOnlyFunc<void(ResultCode, PeerInfo)> completionHandler)
{
    base_type::template makeAsyncCall<PeerInfo>(
        kInfo,
        std::move(completionHandler));
}

std::tuple<Client::ResultCode, PeerInfo> Client::getInfo()
{
    return base_type::template makeSyncCall<PeerInfo>(kInfo);
}

} // namespace nx::clusterdb::engine::api
