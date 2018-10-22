#include "mediator_api_client.h"

#include <nx/fusion/serialization/json.h>
#include <nx/network/http/http_client.h>

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
    ListeningPeersHandler completionHandler)
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

} // namespace api
} // namespace hpm
} // namespace nx
