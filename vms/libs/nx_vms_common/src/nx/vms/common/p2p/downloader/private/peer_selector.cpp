// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "peer_selector.h"

#include <QtCore/QSet>

namespace nx::vms::common::p2p::downloader {

namespace {

template<typename P>
QList<nx::Uuid> filterPeers(
    const QList<PeerSelector::PeerInformation>& peers,
    P predicate,
    const QList<nx::Uuid>& additionalPeers = {})
{
    QSet<nx::Uuid> result(additionalPeers.begin(), additionalPeers.end());
    for (const auto& peer: peers)
    {
        if (predicate(peer))
            result.insert(peer.id);
    }
    return result.values();
}

} // namespace

QList<nx::Uuid> PeerSelector::selectPeers(const QList<PeerSelector::PeerInformation>& peers)
{
    switch (peerSelectionPolicy)
    {
        case FileInformation::all:
            return filterPeers(peers, [](const PeerInformation&) { return true; });

        case FileInformation::byPlatform:
            return filterPeers(peers,
                [this](const PeerInformation& peer) { return osInfo == peer.osInfo; },
                additionalPeers);

        case FileInformation::none:
        default:
            break;
    }
    return {};
}

} // namespace nx::vms::common::p2p::downloader
