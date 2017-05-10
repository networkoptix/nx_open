#pragma once

#include <functional>

#include <QtCore/QHash>
#include <QtCore/QQueue>

#include <nx/vms/common/distributed_file_downloader/private/abstract_peer_manager.h>

namespace nx {
namespace vms {
namespace common {
namespace distributed_file_downloader {

class Storage;

class TestPeerManager: public AbstractPeerManager
{
public:
    struct FileInformation: distributed_file_downloader::FileInformation
    {
        using distributed_file_downloader::FileInformation::FileInformation;
        FileInformation() = default;
        FileInformation(const distributed_file_downloader::FileInformation& fileInfo);

        QString filePath;
        QVector<QByteArray> checksums;
    };

    TestPeerManager();

    void addPeer(const QnUuid& peerId);
    QnUuid addPeer();

    void setFileInformation(const QnUuid& peerId, const FileInformation& fileInformation);
    FileInformation fileInformation(const QnUuid& peerId, const QString& fileName) const;

    void setPeerStorage(const QnUuid& peerId, Storage* storage);
    void setHasInternetConnection(const QnUuid& peerId, bool hasInternetConnection = true);

    QStringList peerGroups(const QnUuid& peerId) const;
    QList<QnUuid> peersInGroup(const QString& group) const;
    void setPeerGroups(const QnUuid& peerId, const QStringList& groups);

    void addInternetFile(const QUrl& url, const QString& fileName);

    void processRequests();

    // AbstractPeerManager implementation
    virtual QnUuid selfId() const override;
    virtual QList<QnUuid> getAllPeers() const override;
    virtual int distanceTo(const QnUuid&) const override;
    virtual bool hasInternetConnection(const QnUuid& peerId) const override;

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

    virtual rest::Handle downloadChunkFromInternet(
        const QnUuid& peerId,
        const QUrl& fileUrl,
        int chunkIndex,
        int chunkSize,
        ChunkCallback callback) override;

    virtual void cancelRequest(const QnUuid& peerId, rest::Handle handle) override;

private:
    using RequestCallback = std::function<void(rest::Handle)>;

    rest::Handle getRequestHandle();
    rest::Handle enqueueRequest(const QnUuid& peerId, RequestCallback callback);

    static QByteArray readFileChunk(const FileInformation& fileInformation, int chunkIndex);

    struct PeerInfo
    {
        QHash<QString, FileInformation> fileInformationByName;
        QStringList groups;
        Storage* storage = nullptr;
        bool hasInternetConnection = false;
    };

    QHash<QnUuid, PeerInfo> m_peers;
    QMultiHash<QString, QnUuid> m_peersByGroup;
    int m_requestIndex = 0;

    QHash<QUrl, QString> m_fileByUrl;

    struct Request
    {
        QnUuid peerId;
        rest::Handle handle;
        RequestCallback callback;
    };

    QQueue<Request> m_requestsQueue;
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
    virtual bool hasInternetConnection(const QnUuid& peerId) const override;

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

    virtual rest::Handle downloadChunkFromInternet(
        const QnUuid& peerId,
        const QUrl& fileUrl,
        int chunkIndex,
        int chunkSize,
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
