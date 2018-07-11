#pragma once

#include <memory>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/system_information.h>

namespace nx {
namespace vms {
namespace common {
namespace p2p {
namespace downloader {
namespace peer_selection {

struct PeerInformation
{
    nx::vms::api::SystemInformation systemInformation;
    QnUuid id;

    PeerInformation(const nx::vms::api::SystemInformation& systemInformation, const QnUuid& id):
        systemInformation(systemInformation),
        id(id)
    {
    }
};

using PeerInformationList = QList<PeerInformation>;

class AbstractPeerSelector
{
public:
    virtual ~AbstractPeerSelector() = default;
    virtual QList<QnUuid> peers(const PeerInformationList& allOtherPeers) const = 0;
};

using AbstractPeerSelectorPtr = std::unique_ptr<AbstractPeerSelector>;

} // namespace peer_selection
} // namespace downloader
} // namespace p2p
} // namespace common
} // namespace vms
} // namespace nx