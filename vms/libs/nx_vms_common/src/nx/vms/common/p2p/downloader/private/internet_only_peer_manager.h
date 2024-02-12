// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/aio/timer.h>

#include "abstract_peer_manager.h"

namespace nx::vms::common::p2p::downloader {

class NX_VMS_COMMON_API InternetOnlyPeerManager: public AbstractPeerManager
{
public:
    InternetOnlyPeerManager();
    virtual ~InternetOnlyPeerManager() override;

    virtual QString peerString(const nx::Uuid& peerId) const override;
    virtual QList<nx::Uuid> getAllPeers() const override;
    virtual QList<nx::Uuid> peers() const override;
    virtual int distanceTo(const nx::Uuid& peerId) const override;

    virtual RequestContextPtr<FileInformation> requestFileInfo(
        const nx::Uuid& peerId,
        const QString& fileName,
        const nx::utils::Url& url) override;

    virtual RequestContextPtr<QVector<QByteArray>> requestChecksums(
        const nx::Uuid& peerId, const QString& fileName) override;

    virtual RequestContextPtr<nx::Buffer> downloadChunk(
        const nx::Uuid& peerId,
        const QString& fileName,
        const nx::utils::Url &url,
        int chunkIndex,
        int chunkSize,
        qint64 fileSize) override;

private:
    nx::network::aio::Timer m_aioTimer;
};

} // namespace nx::vms::common::p2p::downloader
