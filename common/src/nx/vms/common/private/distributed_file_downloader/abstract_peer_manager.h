#pragma once

#include <functional>

#include <nx/utils/uuid.h>
#include <api/server_rest_connection_fwd.h>

#include "../../distributed_file_downloader.h"

namespace nx {
namespace vms {
namespace common {
namespace distributed_file_downloader {

class AbstractPeerManager: public QObject
{
public:
    AbstractPeerManager(QObject* parent = nullptr);
    virtual ~AbstractPeerManager();

    virtual QString peerString(const QnUuid& peerId) const;
    virtual QList<QnUuid> getAllPeers() const = 0;

    using FileInfoCallback =
        std::function<void(bool, rest::Handle, const DownloaderFileInformation&)>;
    virtual rest::Handle requestFileInfo(
        const QnUuid& peer,
        const QString& fileName,
        FileInfoCallback callback) = 0;

    using ChecksumsCallback =
        std::function<void(bool, rest::Handle, const QVector<QByteArray>&)>;
    virtual rest::Handle requestChecksums(
        const QnUuid& peer,
        const QString& fileName,
        ChecksumsCallback callback) = 0;

    using ChunkCallback =
        std::function<void(bool, rest::Handle, const QByteArray&)>;
    virtual rest::Handle downloadChunk(
        const QnUuid& peer,
        const QString& fileName,
        int chunkIndex,
        ChunkCallback callback) = 0;

    virtual void cancelRequest(const QnUuid& peerId, rest::Handle handle) = 0;
};

class AbstractPeerManagerFactory
{
public:
    virtual ~AbstractPeerManagerFactory();
    virtual AbstractPeerManager* createPeerManager() = 0;
};

} // namespace distributed_file_downloader
} // namespace common
} // namespace vms
} // namespace nx
