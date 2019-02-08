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
        RelayInstanceSearchCompletionHandler completionHandler) override;

    virtual void findRelayInstancePeerIsListeningOn(
        const std::string& peerId,
        RelayInstanceSearchCompletionHandler completionHandler) override;

private:
    const conf::Settings& m_settings;
    network::aio::BasicPollable m_aioThreadBinder;
};

} // namespace hpm
} // namespace nx
