#include "client.h"

#include <nx/network/http/rest/http_rest_client.h>

#include "request_path.h"

namespace nx::clusterdb::engine::api {

Client::Client(const nx::utils::Url& baseApiUrl):
    base_type(baseApiUrl)
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
