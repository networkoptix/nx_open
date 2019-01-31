#include <nx/utils/std/cpp14.h>
#include "by_platform_peer_selector.h"

namespace nx {
namespace vms {
namespace common {
namespace p2p {
namespace downloader {
namespace peer_selection {
namespace impl {

ByPlatformPeerSelector::ByPlatformPeerSelector(
    const api::SystemInformation& selfInformation,
    const QList<QnUuid>& additionalPeers)
    :
    m_selfInformation(selfInformation),
    m_additionalPeers(additionalPeers)
{}

QList<QnUuid> ByPlatformPeerSelector::peers(const PeerInformationList& allOtherPeers) const
{
    QList<QnUuid> result = m_additionalPeers;
    for (const auto& peerInfo: allOtherPeers)
    {
        if (peerInfo.systemInformation == m_selfInformation && !result.contains(peerInfo.id))
            result.append(peerInfo.id);
    }

    return result;
}

AbstractPeerSelectorPtr ByPlatformPeerSelector::create(
    const api::SystemInformation& systemInformation,
    const QList<QnUuid>& additionalPeers)
{
    return std::make_unique<ByPlatformPeerSelector>(systemInformation, additionalPeers);
}

} // namespace impl
} //namespace peer_selection
} // namespace downloader
} // namespace p2p
} // namespace common
} // namespace vms
} // namespace nx
