#pragma once

#include <QtCore/QHash>
#include <QtCore/QQueue>

#include <nx/vms/common/private/distributed_file_downloader/abstract_peer_manager.h>

namespace nx {
namespace vms {
namespace common {
namespace distributed_file_downloader {

class TestPeerManager: public AbstractPeerManager
{
public:
    struct FileInformation: DownloaderFileInformation
    {
        using DownloaderFileInformation::DownloaderFileInformation;
        QString filePath;
        QVector<QByteArray> checksums;
    };

    TestPeerManager();

    void addPeer(const QnUuid& peer);
    QnUuid addPeer();

    void setFileInformation(const QnUuid& peer, const FileInformation& fileInformation);
    FileInformation fileInformation(const QnUuid& peer, const QString& fileName) const;

    // AbstractPeerManager implementation
    virtual QnUuid selfId() const override;

    virtual QList<QnUuid> getAllPeers() const override;

    virtual rest::Handle requestFileInfo(
        const QnUuid& peer,
        const QString& fileName,
        FileInfoCallback callback) override;

    virtual rest::Handle requestChecksums(
        const QnUuid& peer,
        const QString& fileName,
        ChecksumsCallback callback) override;

    virtual rest::Handle downloadChunk(
        const QnUuid& peer,
        const QString& fileName,
        int chunkIndex,
        ChunkCallback callback) override;

    virtual void cancelRequest(const QnUuid& peerId, rest::Handle handle) override;

    void processRequests();

private:
    rest::Handle getRequestHandle();
    void enqueueCallback(std::function<void()> callback);

    static QByteArray readFileChunk(const FileInformation& fileInformation, int chunkIndex);

private:
    QnUuid m_selfId;

    struct PeerInfo
    {
        QHash<QString, FileInformation> fileInformationByName;
    };

    QHash<QnUuid, PeerInfo> m_peers;
    int m_requestIndex = 0;

    QQueue<std::function<void()>> m_callbacksQueue;
};

} // namespace distributed_file_downloader
} // namespace common
} // namespace vms
} // namespace nx
