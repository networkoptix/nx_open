#include <nx/utils/std/cpp14.h>
#include "empty_peer_selector.h"

namespace nx {
namespace vms {
namespace common {
namespace p2p {
namespace downloader {
namespace peer_selection {
namespace impl {

QList<QnUuid> EmptyPeerSelector::peers(const PeerInformationList& /*allOtherPeers*/) const
{
    return QList<QnUuid>();
}

AbstractPeerSelectorPtr EmptyPeerSelector::create()
{
    return std::make_unique<EmptyPeerSelector>();
}

} // namespace impl
} //namespace peer_selection
} // namespace downloader
} // namespace p2p
} // namespace common
} // namespace vms
} // namespace nx
