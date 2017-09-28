#pragma once

#include <string>

#include <QtCore/QUrl>

#include <nx/network/cloud/tunnel/relay/api/relay_api_result_code.h>
#include <nx/utils/move_only_func.h>

namespace nx {
namespace hpm {

using RelayInstanceSearchCompletionHandler =
    nx::utils::MoveOnlyFunc<void(cloud::relay::api::ResultCode, QUrl /*relayInstanceUrl*/)>;

/**
 * Every method is thread-safe.
 */
class AbstractRelayClusterClient
{
public:
    virtual ~AbstractRelayClusterClient() = default;

    virtual void selectRelayInstanceForListeningPeer(
        const std::string& peerId,
        RelayInstanceSearchCompletionHandler completionHandler) = 0;
    virtual void findRelayInstancePeerIsListeningOn(
        const std::string& peerId,
        RelayInstanceSearchCompletionHandler completionHandler) = 0;
};

} // namespace hpm
} // namespace nx
