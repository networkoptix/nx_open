#pragma once

#include <nx/utils/uuid.h>
#include <nx/vms/api/data/system_information.h>
#include <nx/vms/common/p2p/downloader/file_information.h>

namespace nx::vms::common::p2p::downloader {

class PeerSelector
{
public:
    struct PeerInformation
    {
        QnUuid id;
        nx::vms::api::SystemInformation systemInformation;
    };

    PeerSelector(
        FileInformation::PeerSelectionPolicy peerSelectionPolicy = FileInformation::all,
        nx::vms::api::SystemInformation systemInformation = {},
        const QList<QnUuid>& additionalPeers = {})
        :
        peerSelectionPolicy(peerSelectionPolicy),
        systemInformation(systemInformation),
        additionalPeers(additionalPeers)
    {
    }

    FileInformation::PeerSelectionPolicy peerSelectionPolicy;
    nx::vms::api::SystemInformation systemInformation;
    QList<QnUuid> additionalPeers;

    QList<QnUuid> selectPeers(const QList<PeerInformation>& peers);
};

} // namespace nx::vms::common::p2p::downloader
