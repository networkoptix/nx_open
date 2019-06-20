#pragma once

#include <nx/utils/url.h>

#include <nx/network/cloud/tunnel/relay/api/relay_api_result_code.h>
#include <nx/utils/move_only_func.h>

namespace nx {
namespace hpm {

using RelayInstanceSelectCompletionHandler =
    nx::utils::MoveOnlyFunc<void(cloud::relay::api::ResultCode, std::vector<nx::utils::Url> /*relayInstanceUrls*/)>;

using RelayInstanceSearchCompletionHandler =
    nx::utils::MoveOnlyFunc<void(cloud::relay::api::ResultCode, nx::utils::Url /*relayInstanceUrl*/)> ;

/**
 * Every method is thread-safe.
 */
class AbstractRelayClusterClient
{
public:
    virtual ~AbstractRelayClusterClient() = default;

    virtual void selectRelayInstanceForListeningPeer(
        const std::string& peerId,
        const nx::network::HostAddress& serverHost,
        RelayInstanceSelectCompletionHandler completionHandler) = 0;

    virtual void findRelayInstanceForClient(
        const std::string& peerId,
        const nx::network::HostAddress& clientHost,
        RelayInstanceSearchCompletionHandler completionHandler) = 0;
};

} // namespace hpm
} // namespace nx
