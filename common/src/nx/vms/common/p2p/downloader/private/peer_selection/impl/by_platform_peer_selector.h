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

class ByPlatformPeerSelector : public AbstractPeerSelector
{
public:
    explicit ByPlatformPeerSelector(
        const QnSystemInformation& systemInformation,
        const QList<QnUuid>& additionalPeers);

    virtual QList<QnUuid> peers(const PeerInformationList& allOtherPeers) const override;
    static AbstractPeerSelectorPtr create(
        const QnSystemInformation& systemInformation,
        const QList<QnUuid>& additionalPeers);

private:
    QnSystemInformation m_selfInformation;
    const QList<QnUuid> m_additionalPeers;
};

} // namespace impl
} //namespace peer_selection
} // namespace downloader
} // namespace p2p
} // namespace common
} // namespace vms
} // namespace nx
