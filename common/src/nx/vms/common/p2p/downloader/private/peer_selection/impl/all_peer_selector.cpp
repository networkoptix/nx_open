#pragma once

#include "all_peer_selector.h"

namespace nx {
namespace vms {
namespace common {
namespace p2p {
namespace downloader {
namespace peer_selection {
namespace impl {

QList<QnUuid> AllPeerSelector::peers(const PeerInformationList& allOtherPeers) const
{
    QList<QnUuid> result;
    for (const auto& peerInfo: allOtherPeers)
        result.append(peerInfo.id);
    return result;
}

AbstractPeerSelectorPtr AllPeerSelector::create()
{
    return std::make_unique<AllPeerSelector>();
}

} // namespace impl
} //namespace peer_selection
} // namespace downloader
} // namespace p2p
} // namespace common
} // namespace vms
} // namespace nx