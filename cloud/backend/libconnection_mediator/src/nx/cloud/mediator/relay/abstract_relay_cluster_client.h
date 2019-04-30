#pragma once

#include <nx/utils/url.h>

#include <nx/network/cloud/tunnel/relay/api/relay_api_result_code.h>
#include <nx/utils/move_only_func.h>

namespace nx {
namespace hpm {

using RelayInstanceSelectCompletionHandler =
    nx::utils::MoveOnlyFunc<void(cloud::relay::api::ResultCode, std::vector<nx::utils::Url>)>;

using RelayInstanceSearchCompletionHandler =
    nx::utils::MoveOnlyFunc<void(cloud::relay::api::ResultCode, nx::utils::Url /*relayInstanceUrl*/) > ;

/**
 * Every method is thread-safe.
 */
class AbstractRelayClusterClient
{
public:
    virtual ~AbstractRelayClusterClient() = default;

    virtual void selectRelayInstanceForListeningPeer(
        const std::string& peerId,
        RelayInstanceSelectCompletionHandler completionHandler) = 0;

    virtual void findRelayInstancePeerIsListeningOn(
        const std::string& peerId,
        RelayInstanceSearchCompletionHandler completionHandler) = 0;
};

} // namespace hpm
} // namespace nx
