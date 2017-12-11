#include "peer_selector_factory.h"
#include "impl/empty_peer_selector.h"
#include "impl/by_platform_peer_selector.h"
#include "impl/all_peer_selector.h"
#include <common/common_module.h>

namespace nx {
namespace vms {
namespace common {
namespace p2p {
namespace downloader {
namespace peer_selection {

namespace {
using namespace impl;

static AbstractPeerSelectorPtr createPeerSelector(
    FileInformation::PeerPolicies peerPolicy,
    QnCommonModule* commonModule)
{
    if (peerPolicy.testFlag(FileInformation::PeerPolicy::all))
        return AllPeersSelector::create();

    if (peerPolicy.testFlag(FileInformation::PeerPolicy::urlOnly))
        return EmptyPeerSelector::create();

    if (peerPolicy.testFlag(FileInformation::PeerPolicy::byPlatform))
        return ByPlatformPeerSelector::create(commonModule->moduleInformation().systemInformation);

    return AbstractPeerSelectorPtr();
}

} // namespace

PeerSelectorFactoryFactoryFunc PeerSelectorFactory::m_peerSelectorFactoryFactoryFunc = nullptr;

AbstractPeerSelectorPtr PeerSelectorFactory::create(
    FileInformation::PeerPolicies peerPolicy,
    QnCommonModule* commonModule)
{
    if (m_peerSelectorFactoryFactoryFunc != nullptr)
        return m_peerSelectorFactoryFactoryFunc(peerPolicy, commonModule);

    return createPeerSelector(peerPolicy, commonModule);
}

void PeerSelectorFactory::setFactoryFunc(
    PeerSelectorFactoryFactoryFunc peerSelectorFactoryFactoryFunc)
{
    m_peerSelectorFactoryFactoryFunc = std::move(peerSelectorFactoryFactoryFunc);
}

} //namespace peer_selection
} // namespace downloader
} // namespace p2p
} // namespace common
} // namespace vms
} // namespace nx