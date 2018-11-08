#pragma once

#include <nx/vms/common/p2p/downloader/private/peer_selection/abstract_peer_selector.h>

namespace nx {
namespace vms {
namespace common {
namespace p2p {
namespace downloader {
namespace peer_selection {
namespace impl {

class ByPlatformPeerSelector : public AbstractPeerSelector
{
public:
    explicit ByPlatformPeerSelector(
        const nx::vms::api::SystemInformation& systemInformation,
        const QList<QnUuid>& additionalPeers);

    virtual QList<QnUuid> peers(const PeerInformationList& allOtherPeers) const override;
    static AbstractPeerSelectorPtr create(
        const nx::vms::api::SystemInformation& systemInformation,
        const QList<QnUuid>& additionalPeers);

private:
    nx::vms::api::SystemInformation m_selfInformation;
    const QList<QnUuid> m_additionalPeers;
};

} // namespace impl
} //namespace peer_selection
} // namespace downloader
} // namespace p2p
} // namespace common
} // namespace vms
} // namespace nx
