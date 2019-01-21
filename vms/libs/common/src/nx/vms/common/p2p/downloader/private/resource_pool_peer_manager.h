#pragma once

#include "abstract_peer_manager.h"

#include <core/resource/resource_fwd.h>
#include <api/server_rest_connection_fwd.h>
#include <common/common_module_aware.h>
#include <nx/vms/common/p2p/downloader/private/abstract_peer_manager.h>
#include <nx/vms/common/p2p/downloader/private/peer_selection/abstract_peer_selector.h>

namespace nx::vms::common::p2p::downloader {

class InternetOnlyPeerManager;

class ResourcePoolPeerManager: public AbstractPeerManager, public QnCommonModuleAware
{
public:
    ResourcePoolPeerManager(
        QnCommonModule* commonModule,
        AbstractPeerSelectorPtr peerSelector, bool isClient = false);

    virtual ~ResourcePoolPeerManager() override;

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

    void setServerDirectConnection(const QnUuid& id, const rest::QnConnectionPtr& connection);
    void stop();

protected:
    virtual QnMediaServerResourcePtr getServer(const QnUuid& peerId) const;
    virtual rest::QnConnectionPtr getConnection(const QnUuid& peerId) const;

private:
    class Private;
    QScopedPointer<Private> d;
};

class ResourcePoolPeerManagerFactory:
    public AbstractPeerManagerFactory,
    public QnCommonModuleAware
{
public:
    ResourcePoolPeerManagerFactory(QnCommonModule* commonModule);
    virtual AbstractPeerManager* createPeerManager(
        FileInformation::PeerSelectionPolicy peerPolicy,
        const QList<QnUuid>& additionalPeers) override;
};

nx::network::http::AsyncHttpClientPtr createHttpClient();

} // namespace nx::vms::common::p2p::downloader
