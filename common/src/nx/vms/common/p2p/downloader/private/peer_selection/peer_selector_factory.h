#pragma once

#include <nx/utils/move_only_func.h>
#include <nx/vms/common/p2p/downloader/file_information.h>
#include <nx/vms/common/p2p/downloader/private/peer_selection/abstract_peer_selector.h>

class QnCommonModule;

namespace nx {
namespace vms {
namespace common {
namespace p2p {
namespace downloader {
namespace peer_selection {

using PeerSelectorFactoryFactoryFunc =
    utils::MoveOnlyFunc<AbstractPeerSelectorPtr(
        FileInformation::PeerPolicies,
        QnCommonModule* commonModule)>;

class PeerSelectorFactory
{
public:
    static AbstractPeerSelectorPtr create(
        FileInformation::PeerPolicies peerPolicy,
        QnCommonModule* commonModule);
    static void setFactoryFunc(PeerSelectorFactoryFactoryFunc peerSelectorFactoryFactoryFunc);
private:
    static PeerSelectorFactoryFactoryFunc m_peerSelectorFactoryFactoryFunc;
};

} //namespace peer_selection
} // namespace downloader
} // namespace p2p
} // namespace common
} // namespace vms
} // namespace nx