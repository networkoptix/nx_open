#pragma once

#include "abstract_peer_manager.h"

#include <core/resource/resource_fwd.h>
#include <api/server_rest_connection_fwd.h>
#include <common/common_module_aware.h>

class QnResourcePool;

namespace nx {
namespace vms {
namespace common {
namespace distributed_file_downloader {

class ResourcePoolPeerManager: public AbstractPeerManager, public QnCommonModuleAware
{
public:
    ResourcePoolPeerManager(QnCommonModule* commonModule);

    virtual QnUuid selfId() const override;

    virtual QString peerString(const QnUuid& peerId) const override;
    virtual QList<QnUuid> getAllPeers() const override;
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

    virtual rest::Handle downloadChunkFromInternet(
        const QnUuid& peerId,
        const QUrl& fileUrl,
        int chunkIndex,
        int chunkSize,
        ChunkCallback callback) override;

    virtual void cancelRequest(const QnUuid& peerId, rest::Handle handle) override;

private:
    QnMediaServerResourcePtr getServer(const QnUuid& peerId) const;
    rest::QnConnectionPtr getConnection(const QnUuid& peerId) const;
};

class ResourcePoolPeerManagerFactory:
    public AbstractPeerManagerFactory,
    public QnCommonModuleAware
{
public:
    ResourcePoolPeerManagerFactory(QnCommonModule* commonModule);
    virtual AbstractPeerManager* createPeerManager() override;
};

} // namespace distributed_file_downloader
} // namespace common
} // namespace vms
} // namespace nx
