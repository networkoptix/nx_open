// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <api/server_rest_connection_fwd.h>
#include <core/resource/resource_fwd.h>
#include <nx/vms/common/p2p/downloader/private/abstract_peer_manager.h>
#include <nx/vms/common/p2p/downloader/private/peer_selector.h>
#include <nx/vms/common/system_context_aware.h>

#include "abstract_peer_manager.h"

namespace nx::vms::common::p2p::downloader {

class NX_VMS_COMMON_API ResourcePoolPeerManager:
    public AbstractPeerManager,
    public SystemContextAware
{
public:
    ResourcePoolPeerManager(SystemContext* systemContext, const PeerSelector& peerSelector = {});

    virtual ~ResourcePoolPeerManager() override;

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

    void setServerDirectConnection(const nx::Uuid& id, const rest::ServerConnectionPtr& connection);
    void clearServerDirectConnections();

protected:
    ResourcePoolPeerManager(
        Capabilities capabilities,
        SystemContext* systemContext,
        const PeerSelector& peerSelector = {});

    virtual QnMediaServerResourcePtr getServer(const nx::Uuid& peerId) const;
    virtual rest::ServerConnectionPtr getConnection(const nx::Uuid& peerId) const;

    void setIndirectInternetRequestsAllowed(bool allow);

private:
    class Private;
    QScopedPointer<Private> d;
};

class NX_VMS_COMMON_API ResourcePoolProxyPeerManager: public ResourcePoolPeerManager
{
public:
    ResourcePoolProxyPeerManager(
        SystemContext* systemContext,
        const PeerSelector& peerSelector = {});
};

} // namespace nx::vms::common::p2p::downloader
