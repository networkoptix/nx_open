#pragma once

#include <nx/network/aio/basic_pollable.h>

#include "abstract_relay_cluster_client.h"
#include "../settings.h"

namespace nx {
namespace hpm {

class RelayClusterClient:
    public AbstractRelayClusterClient
{
public:
    RelayClusterClient(const conf::Settings& settings);
    virtual ~RelayClusterClient() override;

    virtual void selectRelayInstanceForListeningPeer(
        const std::string& peerId,
        const nx::network::HostAddress& serverHost,
        RelayInstanceSelectCompletionHandler completionHandler) override;

    virtual void findRelayInstanceForClient(
        const std::string& peerId,
        const nx::network::HostAddress& clientHost,
        RelayInstanceSearchCompletionHandler completionHandler) override;

private:
    const conf::Settings& m_settings;
    network::aio::BasicPollable m_aioThreadBinder;
};

} // namespace hpm
} // namespace nx
