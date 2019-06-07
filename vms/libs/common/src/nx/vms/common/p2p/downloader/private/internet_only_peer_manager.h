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

    virtual RequestContext<FileInformation> requestFileInfo(
        const QnUuid& peerId,
        const QString& fileName,
        const nx::utils::Url& url) override;

    virtual RequestContext<QVector<QByteArray>> requestChecksums(
        const QnUuid& peerId, const QString& fileName) override;

    virtual RequestContext<QByteArray> downloadChunk(
        const QnUuid& peerId,
        const QString& fileName,
        const nx::utils::Url &url,
        int chunkIndex,
        int chunkSize) override;

private:
    network::aio::Timer m_aioTimer;
};

} // namespace nx::vms::common::p2p::downloader
