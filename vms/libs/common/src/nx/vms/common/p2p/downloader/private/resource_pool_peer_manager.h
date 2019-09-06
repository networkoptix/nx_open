#pragma once

#include "abstract_peer_manager.h"

#include <core/resource/resource_fwd.h>
#include <api/server_rest_connection_fwd.h>
#include <common/common_module_aware.h>
#include <nx/vms/common/p2p/downloader/private/abstract_peer_manager.h>
#include <nx/vms/common/p2p/downloader/private/peer_selector.h>

namespace nx::vms::common::p2p::downloader {

class ResourcePoolPeerManager: public AbstractPeerManager, public /*mixin*/ QnCommonModuleAware
{
public:
    ResourcePoolPeerManager(QnCommonModule* commonModule, const PeerSelector& peerSelector = {});

    virtual ~ResourcePoolPeerManager() override;

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

    void setServerDirectConnection(const QnUuid& id, const rest::QnConnectionPtr& connection);
    QnUuid getServerIdWithInternet() const;
    void setPeersWithInternetAccess(const QSet<QnUuid>& ids);

protected:
    ResourcePoolPeerManager(
        Capabilities capabilities,
        QnCommonModule* commonModule,
        const PeerSelector& peerSelector = {});

    virtual QnMediaServerResourcePtr getServer(const QnUuid& peerId) const;
    virtual rest::QnConnectionPtr getConnection(const QnUuid& peerId) const;

    void setIndirectInternetRequestsAllowed(bool allow);

private:
    class Private;
    QScopedPointer<Private> d;
};

class ResourcePoolProxyPeerManager: public ResourcePoolPeerManager
{
public:
    ResourcePoolProxyPeerManager(
        QnCommonModule* commonModule, const PeerSelector& peerSelector = {});
};

} // namespace nx::vms::common::p2p::downloader
