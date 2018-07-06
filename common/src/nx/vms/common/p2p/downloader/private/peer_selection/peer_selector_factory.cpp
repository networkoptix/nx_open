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
    FileInformation::PeerSelectionPolicy peerPolicy,
    QnCommonModule* commonModule)
{
    switch (peerPolicy)
    {
        case FileInformation::PeerSelectionPolicy::none:
            return EmptyPeerSelector::create();
        case FileInformation::PeerSelectionPolicy::byPlatform:
            return ByPlatformPeerSelector::create(
                commonModule->moduleInformation().systemInformation);
        case FileInformation::PeerSelectionPolicy::all:
            return AllPeerSelector::create();
        default:
            return AbstractPeerSelectorPtr();
    }
}

} // namespace

PeerSelectorFactoryFactoryFunc PeerSelectorFactory::m_peerSelectorFactoryFactoryFunc = nullptr;

AbstractPeerSelectorPtr PeerSelectorFactory::create(
    FileInformation::PeerSelectionPolicy peerPolicy,
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