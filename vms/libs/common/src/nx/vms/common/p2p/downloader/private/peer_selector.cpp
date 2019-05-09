#include "peer_selector.h"

#include <QtCore/QSet>

namespace nx::vms::common::p2p::downloader {

namespace {

template<typename P>
QList<QnUuid> filterPeers(
    const QList<PeerSelector::PeerInformation>& peers,
    P predicate,
    const QList<QnUuid>& additionalPeers = {})
{
    QSet<QnUuid> result = additionalPeers.toSet();
    for (const auto& peer: peers)
    {
        if (predicate(peer))
            result.insert(peer.id);
    }
    return result.toList();
}

} // namespace

QList<QnUuid> PeerSelector::selectPeers(const QList<PeerSelector::PeerInformation>& peers)
{
    switch (peerSelectionPolicy)
    {
        case FileInformation::all:
            return filterPeers(peers, [](const PeerInformation&) { return true; });

        case FileInformation::byPlatform:
            return filterPeers(peers,
                [this](const PeerInformation& peer)
                {
                    return systemInformation == peer.systemInformation;
                }, additionalPeers);

        case FileInformation::none:
        default:
            break;
    }
    return {};
}

} // namespace nx::vms::common::p2p::downloader
