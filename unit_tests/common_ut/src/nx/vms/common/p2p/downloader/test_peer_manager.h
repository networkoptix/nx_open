#pragma once

#include <array>
#include <functional>

#include <QtCore/QHash>
#include <QtCore/QQueue>

#include <nx/vms/common/p2p/downloader/private/abstract_peer_manager.h>
#include <nx/utils/thread/long_runnable.h>

namespace nx {
namespace vms {
namespace common {
namespace p2p {
namespace downloader {

class Storage;
class TestPeerManager;

struct RequestCounter
{
    enum RequestType
    {
        FirstRequestType,

        FileInfoRequest = FirstRequestType,
        ChecksumsRequest,
        DownloadChunkRequest,
        DownloadChunkFromInternetRequest,
        InternetDownloadRequestsPerformed,
        Total,

        RequestTypesCount
    };

    void incrementCounters(const QnUuid& peerId, RequestType requestType);

    int totalRequests() const;

    void printCounters(const QString& header, TestPeerManager* peerManager) const;

    static QString requestTypeShortName(RequestType requestType);

    std::array<QHash<QnUuid, int>, RequestTypesCount> counters;
};

class TestPeerManager: public AbstractPeerManager, public QnLongRunnable
{
public:
    struct FileInformation: downloader::FileInformation
    {
        using downloader::FileInformation::FileInformation;
        FileInformation() = default;
        FileInformation(const downloader::FileInformation& fileInfo);

        QString filePath;
        QVector<QByteArray> checksums;
    };

    TestPeerManager();

    void setPeerList(const QList<QnUuid>& peerList) { m_peerList = peerList; }
    void addPeer(const QnUuid& peerId, const QString& peerName = QString());
    QnUuid addPeer(const QString& peerName = QString());

    void setFileInformation(const QnUuid& peerId, const FileInformation& fileInformation);
    FileInformation fileInformation(const QnUuid& peerId, const QString& fileName) const;

    void setPeerStorage(const QnUuid& peerId, Storage* storage);
    void setHasInternetConnection(const QnUuid& peerId, bool hasInternetConnection = true);

    QStringList peerGroups(const QnUuid& peerId) const;
    QList<QnUuid> peersInGroup(const QString& group) const;
    void setPeerGroups(const QnUuid& peerId, const QStringList& groups);

    void addInternetFile(const nx::utils::Url& url, const QString& fileName);

    bool processNextRequest();
    void exec(int maxRequests = 0);

    const RequestCounter* requestCounter() const;

    using WaitCallback = std::function<void()>;
    void requestWait(const QnUuid& peerId, qint64 waitTime, WaitCallback callback);

    // AbstractPeerManager implementation
    virtual QnUuid selfId() const override;
    virtual QString peerString(const QnUuid& peerId) const;
    virtual QList<QnUuid> getAllPeers() const override;
    virtual QList<QnUuid> peers() const override { return m_peerList; }
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
        const QString& fileName,
        const nx::utils::Url& url,
        int chunkIndex,
        int chunkSize,
        ChunkCallback callback) override;

    virtual rest::Handle validateFileInformation(
        const downloader::FileInformation& fileInformation, ValidateCallback callback) override;

    virtual void cancelRequest(const QnUuid& peerId, rest::Handle handle) override;

    virtual void pleaseStop() override;
    virtual void run() override;


private:
    using RequestCallback = std::function<void(rest::Handle)>;

    rest::Handle getRequestHandle();
    rest::Handle enqueueRequest(const QnUuid& peerId, qint64 time, RequestCallback callback);

    static QByteArray readFileChunk(const FileInformation& fileInformation, int chunkIndex);

    struct PeerInfo
    {
        QString name;
        QHash<QString, FileInformation> fileInformationByName;
        QStringList groups;
        Storage* storage = nullptr;
        bool hasInternetConnection = false;
    };

    QHash<QnUuid, PeerInfo> m_peers;
    QMultiHash<QString, QnUuid> m_peersByGroup;
    int m_requestIndex = 0;

    QHash<nx::utils::Url, QString> m_fileByUrl;
    QList<QnUuid> m_peerList;

    struct Request
    {
        QnUuid peerId;
        rest::Handle handle;
        RequestCallback callback;
        qint64 timeToReply;
    };

    QQueue<Request> m_requestsQueue;
    QnMutex m_mutex;
    QnWaitCondition m_condition;
    qint64 m_currentTime = 0;
    RequestCounter m_requestCounter;
};

class ProxyTestPeerManager: public AbstractPeerManager
{
public:
    ProxyTestPeerManager(TestPeerManager* peerManager, const QString& peerName = QString());
    ProxyTestPeerManager(
        TestPeerManager* peerManager, const QnUuid& id, const QString& peerName = QString());

    void calculateDistances();

    const RequestCounter* requestCounter() const;

    void requestWait(qint64 waitTime, TestPeerManager::WaitCallback callback);
    void setPeerList(const QList<QnUuid>& peerList) { m_peerList = peerList; }

    // AbstractPeerManager implementation
    virtual QnUuid selfId() const override;
    virtual QString peerString(const QnUuid& peerId) const;
    virtual QList<QnUuid> getAllPeers() const override;
    virtual QList<QnUuid> peers() const override { return m_peerList; }
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
        const QString& fileName,
        const nx::utils::Url& url,
        int chunkIndex,
        int chunkSize,
        ChunkCallback callback) override;

    virtual rest::Handle validateFileInformation(
        const FileInformation& fileInformation, ValidateCallback callback) override;

    virtual void cancelRequest(const QnUuid& peerId, rest::Handle handle) override;

private:
    TestPeerManager* m_peerManager;
    QnUuid m_selfId;
    QHash<QnUuid, int> m_distances;
    RequestCounter m_requestCounter;
    QList<QnUuid> m_peerList;
};

} // namespace downloader
} // namespace p2p
} // namespace common
} // namespace vms
} // namespace nx
