#pragma once

#include <nx/utils/uuid.h>
#include <nx/utils/os_info.h>
#include <nx/vms/common/p2p/downloader/file_information.h>

namespace nx::vms::common::p2p::downloader {

class PeerSelector
{
public:
    struct PeerInformation
    {
        QnUuid id;
        nx::utils::OsInfo osInfo;
    };

    PeerSelector(
        FileInformation::PeerSelectionPolicy peerSelectionPolicy = FileInformation::all,
        const nx::utils::OsInfo& osInfo = {},
        const QList<QnUuid>& additionalPeers = {})
        :
        peerSelectionPolicy(peerSelectionPolicy),
        osInfo(osInfo),
        additionalPeers(additionalPeers)
    {
    }

    FileInformation::PeerSelectionPolicy peerSelectionPolicy;
    nx::utils::OsInfo osInfo;
    QList<QnUuid> additionalPeers;

    QList<QnUuid> selectPeers(const QList<PeerInformation>& peers);
};

} // namespace nx::vms::common::p2p::downloader
