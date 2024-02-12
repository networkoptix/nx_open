// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/os_info.h>
#include <nx/utils/uuid.h>
#include <nx/vms/common/p2p/downloader/file_information.h>

namespace nx::vms::common::p2p::downloader {

class PeerSelector
{
public:
    struct PeerInformation
    {
        nx::Uuid id;
        nx::utils::OsInfo osInfo;
    };

    PeerSelector(
        FileInformation::PeerSelectionPolicy peerSelectionPolicy = FileInformation::all,
        const nx::utils::OsInfo& osInfo = {},
        const QList<nx::Uuid>& additionalPeers = {})
        :
        peerSelectionPolicy(peerSelectionPolicy),
        osInfo(osInfo),
        additionalPeers(additionalPeers)
    {
    }

    FileInformation::PeerSelectionPolicy peerSelectionPolicy;
    nx::utils::OsInfo osInfo;
    QList<nx::Uuid> additionalPeers;

    QList<nx::Uuid> selectPeers(const QList<PeerInformation>& peers);
};

} // namespace nx::vms::common::p2p::downloader
