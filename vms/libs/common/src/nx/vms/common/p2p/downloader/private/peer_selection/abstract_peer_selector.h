#pragma once

#include <memory>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/system_information.h>
#include <nx/vms/common/p2p/downloader/file_information.h>

class QnCommonModule;

namespace nx::vms::common::p2p::downloader {

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

AbstractPeerSelectorPtr createPeerSelector(
    FileInformation::PeerSelectionPolicy peerPolicy,
    const QList<QnUuid>& additionalPeers,
    QnCommonModule* commonModule);

} // namespace nx::vms::common::p2p::downloader
