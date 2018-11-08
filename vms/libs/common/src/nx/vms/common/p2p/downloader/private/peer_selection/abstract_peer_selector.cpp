#include "abstract_peer_selector.h"
#include "impl/empty_peer_selector.h"
#include "impl/by_platform_peer_selector.h"
#include "impl/all_peer_selector.h"
#include <common/common_module.h>

namespace nx::vms::common::p2p::downloader {

AbstractPeerSelectorPtr createPeerSelector(
    FileInformation::PeerSelectionPolicy peerPolicy,
    const QList<QnUuid>& additionalPeers,
    QnCommonModule* commonModule)
{
    switch (peerPolicy)
    {
        case FileInformation::PeerSelectionPolicy::none:
            return peer_selection::impl::EmptyPeerSelector::create();
        case FileInformation::PeerSelectionPolicy::byPlatform:
            return peer_selection::impl::ByPlatformPeerSelector::create(
                commonModule->moduleInformation().systemInformation,
                additionalPeers);
        case FileInformation::PeerSelectionPolicy::all:
            return peer_selection::impl::AllPeerSelector::create();
        default:
            return AbstractPeerSelectorPtr();
    }
}

} // namespace nx::vms::common::p2p::downloader
