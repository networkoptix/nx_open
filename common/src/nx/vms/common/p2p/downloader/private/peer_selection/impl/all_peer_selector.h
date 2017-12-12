#pragma once


#include <nx/vms/common/p2p/downloader/private/peer_selection/abstract_peer_selector.h>
#include "network/module_information.h"

namespace nx {
namespace vms {
namespace common {
namespace p2p {
namespace downloader {
namespace peer_selection {
namespace impl {

class AllPeerSelector : public AbstractPeerSelector
{
public:
    virtual QList<QnUuid> peers(const PeerInformationList& allOtherPeers) const override;
    static AbstractPeerSelectorPtr create();
};

} // namespace impl
} //namespace peer_selection
} // namespace downloader
} // namespace p2p
} // namespace common
} // namespace vms
} // namespace nx