#pragma once

#include "abstract_peer_manager.h"
#include <nx/network/aio/timer.h>

namespace nx::vms::common::p2p::downloader {

class InternetOnlyPeerManager: public AbstractPeerManager
{
public:
    InternetOnlyPeerManager();
    virtual ~InternetOnlyPeerManager() override;

    virtual QnUuid selfId() const override;
    virtual QString peerString(const QnUuid& peerId) const override;
    virtual QList<QnUuid> getAllPeers() const override;
    virtual QList<QnUuid> peers() const override;
    virtual int distanceTo(const QnUuid& peerId) const override;
    virtual bool hasInternetConnection(const QnUuid& peerId) const override;

    virtual rest::Handle requestFileInfo(
        const QnUuid& peerId,
        const QString& fileName,
        FileInfoCallback callback) override;

    virtual rest::Handle requestChecksums(
        const QnUuid& peerId,
        const QString& fileName,
        ChecksumsCallback callback) override;

    virtual rest::Handle downloadChunk(
        const QnUuid& peerId,
        const QString& fileName,
        int chunkIndex,
        ChunkCallback callback) override;

    virtual rest::Handle downloadChunkFromInternet(const QnUuid& peerId,
        const QString& fileName,
        const nx::utils::Url &url,
        int chunkIndex,
        int chunkSize,
        ChunkCallback callback) override;

    virtual void cancelRequest(const QnUuid& peerId, rest::Handle handle) override;
    virtual bool hasAccessToTheUrl(const QString& url) const override;
    virtual void stop() override;

private:
    class Private;
    const QScopedPointer<Private> d;
    bool m_stopped = false;
    network::aio::Timer m_aioTimer;
};

class InternetOnlyPeerManagerFactory: public AbstractPeerManagerFactory
{
public:
    virtual AbstractPeerManager* createPeerManager(
        FileInformation::PeerSelectionPolicy peerPolicy,
        const QList<QnUuid>& additionalPeers) override;
};

} // namespace nx::vms::common::p2p::downloader
