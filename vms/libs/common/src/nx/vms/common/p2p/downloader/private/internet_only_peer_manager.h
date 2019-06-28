#pragma once

#include <nx/network/aio/timer.h>

#include "abstract_peer_manager.h"

namespace nx::vms::common::p2p::downloader {

class InternetOnlyPeerManager: public AbstractPeerManager
{
public:
    InternetOnlyPeerManager();
    virtual ~InternetOnlyPeerManager() override;

    virtual QString peerString(const QnUuid& peerId) const override;
    virtual QList<QnUuid> getAllPeers() const override;
    virtual QList<QnUuid> peers() const override;
    virtual int distanceTo(const QnUuid& peerId) const override;

    virtual RequestContextPtr<FileInformation> requestFileInfo(
        const QnUuid& peerId,
        const QString& fileName,
        const nx::utils::Url& url) override;

    virtual RequestContextPtr<QVector<QByteArray>> requestChecksums(
        const QnUuid& peerId, const QString& fileName) override;

    virtual RequestContextPtr<QByteArray> downloadChunk(
        const QnUuid& peerId,
        const QString& fileName,
        const nx::utils::Url &url,
        int chunkIndex,
        int chunkSize) override;

private:
    network::aio::Timer m_aioTimer;
};

} // namespace nx::vms::common::p2p::downloader
