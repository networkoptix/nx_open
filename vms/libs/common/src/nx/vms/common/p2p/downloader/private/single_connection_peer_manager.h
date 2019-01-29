#pragma once
#include <common/common_module_aware.h>
#include <nx/vms/common/p2p/downloader/downloader.h>
#include <nx/vms/common/p2p/downloader/private/abstract_peer_manager.h>
#include <nx/vms/common/p2p/downloader/private/resource_pool_peer_manager.h>
#include <nx/vms/common/p2p/downloader/private/peer_selection/abstract_peer_selector.h>

namespace nx::vms::common::p2p::downloader {

/**
 * Peer manager to be used in Downloader class. It is intended for client-only usage.
 */
class SingleConnectionPeerManager: public AbstractPeerManager, public /*mixin*/ QnCommonModuleAware
{
public:
    SingleConnectionPeerManager(
        QnCommonModule* commonModule,
        AbstractPeerSelectorPtr&& peerSelector);
    ~SingleConnectionPeerManager();

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
    virtual void cancel() override {}

    // Sets internal connection
    void setServerUrl(nx::utils::Url serverUrl, QnUuid serverId);
    QnUuid getServerId() const;

protected:
    virtual rest::QnConnectionPtr getConnection(const QnUuid& peerId) const;

    rest::Handle m_currentSelfRequestHandle = 0;
    QHash<rest::Handle, nx::network::http::AsyncHttpClientPtr> m_httpClientByHandle;
    AbstractPeerSelectorPtr m_peerSelector;

    rest::QnConnectionPtr m_directConnection;
    nx::utils::Url m_directUrl;
};

} // namespace nx::vms::common::p2p::downloader
