#pragma once

#include <functional>

#include <nx/utils/uuid.h>
#include <nx/utils/url.h>
#include <api/server_rest_connection_fwd.h>

#include <nx/vms/common/p2p/downloader/file_information.h>

namespace nx {
namespace vms {
namespace common {
namespace p2p {
namespace downloader {

class AbstractPeerManager: public QObject
{
public:
    AbstractPeerManager(QObject* parent = nullptr);
    virtual ~AbstractPeerManager();

    virtual QnUuid selfId() const = 0;

    /** @return Human readable peer name. This is mostly used in logs. */
    virtual QString peerString(const QnUuid& peerId) const;
    virtual QList<QnUuid> getAllPeers() const = 0;
    virtual QList<QnUuid> peers() const = 0;
    virtual int distanceTo(const QnUuid& peerId) const = 0;
    virtual bool hasInternetConnection(const QnUuid& peerId) const = 0;

    using FileInfoCallback =
        std::function<void(bool, rest::Handle, const FileInformation&)>;
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

    virtual rest::Handle downloadChunkFromInternet(
        const QnUuid& peerId,
        const QString& fileName,
        const nx::utils::Url& url,
        int chunkIndex,
        int chunkSize,
        ChunkCallback callback) = 0;

    virtual void cancel() = 0;

    virtual void cancelRequest(const QnUuid& peerId, rest::Handle handle) = 0;
    virtual bool hasAccessToTheUrl(const QString& url) const = 0;
};

class AbstractPeerManagerFactory
{
public:
    virtual ~AbstractPeerManagerFactory();
    virtual AbstractPeerManager* createPeerManager(
        FileInformation::PeerSelectionPolicy peerPolicy,
        const QList<QnUuid>& additionalPeers) = 0;
};

/*
How it should really be
using PeerManagerFactory = std::function<AbstractPeerManager*(
        FileInformation::PeerSelectionPolicy policy,
        const QList<QnUuid>& peers)>;*/

} // namespace downloader
} // namespace p2p
} // namespace common
} // namespace vms
} // namespace nx
