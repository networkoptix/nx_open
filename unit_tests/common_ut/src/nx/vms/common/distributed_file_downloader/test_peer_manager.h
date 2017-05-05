#pragma once

#include <QtCore/QHash>
#include <QtCore/QQueue>

#include <nx/vms/common/private/distributed_file_downloader/abstract_peer_manager.h>

namespace nx {
namespace vms {
namespace common {
namespace distributed_file_downloader {

class Storage;

class TestPeerManager: public AbstractPeerManager
{
public:
    struct FileInformation: DownloaderFileInformation
    {
        using DownloaderFileInformation::DownloaderFileInformation;
        FileInformation() = default;
        FileInformation(const DownloaderFileInformation& fileInfo);

        QString filePath;
        QVector<QByteArray> checksums;
    };

    TestPeerManager();

    void addPeer(const QnUuid& peerId);
    QnUuid addPeer();

    void setFileInformation(const QnUuid& peerId, const FileInformation& fileInformation);
    FileInformation fileInformation(const QnUuid& peerId, const QString& fileName) const;

    void setPeerStorage(const QnUuid& peerId, Storage* storage);

    QStringList peerGroups(const QnUuid& peerId) const;
    QList<QnUuid> peersInGroup(const QString& group) const;
    void setPeerGroups(const QnUuid& peerId, const QStringList& groups);

    void processRequests();

    // AbstractPeerManager implementation
    virtual QnUuid selfId() const override;

    virtual QList<QnUuid> getAllPeers() const override;

    virtual int distanceTo(const QnUuid&) const override;

    virtual rest::Handle requestFileInfo(
        const QnUuid& peer,
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

    virtual void cancelRequest(const QnUuid& peerId, rest::Handle handle) override;

private:
    rest::Handle getRequestHandle();
    void enqueueCallback(std::function<void()> callback);

    static QByteArray readFileChunk(const FileInformation& fileInformation, int chunkIndex);

private:
    struct PeerInfo
    {
        QHash<QString, FileInformation> fileInformationByName;
        QStringList groups;
        Storage* storage = nullptr;
    };

    QHash<QnUuid, PeerInfo> m_peers;
    QMultiHash<QString, QnUuid> m_peersByGroup;
    int m_requestIndex = 0;

    QQueue<std::function<void()>> m_callbacksQueue;
};

class ProxyTestPeerManager: public AbstractPeerManager
{
public:
    ProxyTestPeerManager(TestPeerManager* peerManager);
    ProxyTestPeerManager(TestPeerManager* peerManager, const QnUuid& id);

    void calculateDistances();

    // AbstractPeerManager implementation
    virtual QnUuid selfId() const override;

    virtual QList<QnUuid> getAllPeers() const override;

    virtual int distanceTo(const QnUuid&) const override;

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

private:
    TestPeerManager* m_peerManager;
    QnUuid m_selfId;
    QHash<QnUuid, int> m_distances;
};

} // namespace distributed_file_downloader
} // namespace common
} // namespace vms
} // namespace nx
