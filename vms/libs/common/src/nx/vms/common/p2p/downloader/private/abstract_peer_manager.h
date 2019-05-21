#pragma once

#include <functional>

#include <nx/utils/uuid.h>
#include <nx/utils/url.h>
#include <api/server_rest_connection_fwd.h>

#include <nx/vms/common/p2p/downloader/file_information.h>

namespace nx::vms::common::p2p::downloader {

class AbstractPeerManager: public QObject
{
public:
    enum Capability
    {
        FileInfo = 1 << 0,
        Checksums = 1 << 1,
        DownloadChunk = 1 << 2,
        AllCapabilities = FileInfo | Checksums | DownloadChunk
    };
    Q_DECLARE_FLAGS(Capabilities, Capability)

    AbstractPeerManager(Capabilities capabilities, QObject* parent = nullptr):
        QObject(parent),
        capabilities(capabilities)
    {
    }

    virtual ~AbstractPeerManager() = default;

    /** @return Human readable peer name. This is mostly used in logs. */
    virtual QString peerString(const QnUuid& peerId) const { return peerId.toString(); }
    virtual QList<QnUuid> getAllPeers() const = 0;
    virtual QList<QnUuid> peers() const = 0;
    virtual int distanceTo(const QnUuid& peerId) const = 0;

    using FileInfoCallback =
        std::function<void(bool, rest::Handle, const FileInformation&)>;
    virtual rest::Handle requestFileInfo(
        const QnUuid& peer,
        const QString& fileName,
        const nx::utils::Url& url,
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
        const QnUuid& peerId,
        const QString& fileName,
        const nx::utils::Url& url,
        int chunkIndex,
        int chunkSize,
        ChunkCallback callback) = 0;

    virtual void cancelRequest(const QnUuid& peerId, rest::Handle handle) = 0;

    const Capabilities capabilities;
};

} // namespace nx::vms::common::p2p::downloader
