// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <array>
#include <functional>

#include <QtCore/QHash>
#include <QtCore/QQueue>

#include <nx/vms/common/p2p/downloader/private/abstract_peer_manager.h>
#include <nx/utils/thread/long_runnable.h>

namespace nx::vms::common::p2p::downloader {

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

private:
    std::array<QHash<QnUuid, int>, RequestTypesCount> counters;
    mutable nx::Mutex m_mutex;
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
    virtual ~TestPeerManager() override { stop(); }

    void setPeerList(const QList<QnUuid>& peerList) { m_peerList = peerList; }
    void addPeer(const QnUuid& peerId, const QString& peerName = QString());
    QnUuid addPeer(const QString& peerName = QString());

    void setFileInformation(const QnUuid& peerId, const FileInformation& fileInformation);
    FileInformation fileInformation(const QnUuid& peerId, const QString& fileName) const;

    void setPeerStorage(const QnUuid& peerId, Storage* storage);
    void setHasInternetConnection(const QnUuid& peerId, bool hasInternetConnection = true);
    bool hasInternetConnection(const QnUuid& peerId) const;

    QStringList peerGroups(const QnUuid& peerId) const;
    QList<QnUuid> peersInGroup(const QString& group) const;
    void setPeerGroups(const QnUuid& peerId, const QStringList& groups);

    void addInternetFile(const nx::utils::Url& url, const QString& fileName);

    const RequestCounter* requestCounter() const;

    using WaitCallback = std::function<void()>;

    void setIndirectInternetRequestsAllowed(bool allow);

    // AbstractPeerManager implementation
    virtual QString peerString(const QnUuid& peerId) const override;
    virtual QList<QnUuid> getAllPeers() const override;
    virtual QList<QnUuid> peers() const override { return m_peerList; }
    virtual int distanceTo(const QnUuid&) const override;

    virtual RequestContextPtr<downloader::FileInformation> requestFileInfo(
        const QnUuid& peer,
        const QString& fileName,
        const nx::utils::Url& url) override;

    virtual RequestContextPtr<QVector<QByteArray>> requestChecksums(
        const QnUuid& peerId, const QString& fileName) override;

    virtual RequestContextPtr<nx::Buffer> downloadChunk(
        const QnUuid& peerId,
        const QString& fileName,
        const nx::utils::Url& url,
        int chunkIndex,
        int chunkSize,
        qint64 fileSize) override;

    virtual void cancelRequest(const QnUuid& peerId, rest::Handle handle);

    virtual void pleaseStop() override;
    virtual void run() override;

    void setDelayBeforeRequest(qint64 delay);
    void setValidateShouldFail();
    void setOneShotDownloadFail();

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

    bool m_allowIndirectInternetRequests = false;

    struct Request
    {
        QnUuid peerId;
        rest::Handle handle;
        RequestCallback callback;
        qint64 timeToReply;
    };

    QQueue<Request> m_requestsQueue;
    nx::Mutex m_mutex;
    nx::WaitCondition m_condition;
    qint64 m_currentTime = 0;
    RequestCounter m_requestCounter;
    qint64 m_delayBeforeRequest = 0;
    bool m_validateShouldFail = false;
    bool m_downloadFailed = false;
};

class ProxyTestPeerManager: public AbstractPeerManager
{
public:
    ProxyTestPeerManager(TestPeerManager* peerManager, const QString& peerName = QString());
    ProxyTestPeerManager(
        TestPeerManager* peerManager, const QnUuid& id, const QString& peerName = QString());

    void calculateDistances();

    const RequestCounter* requestCounter() const;

    void setPeerList(const QList<QnUuid>& peerList) { m_peerList = peerList; }

    // AbstractPeerManager implementation
    virtual QString peerString(const QnUuid& peerId) const override;
    virtual QList<QnUuid> getAllPeers() const override;
    virtual QList<QnUuid> peers() const override { return m_peerList; }
    virtual int distanceTo(const QnUuid&) const override;

    virtual RequestContextPtr<FileInformation> requestFileInfo(
        const QnUuid& peer,
        const QString& fileName,
        const nx::utils::Url& url) override;

    virtual RequestContextPtr<QVector<QByteArray>> requestChecksums(
        const QnUuid& peer, const QString& fileName) override;

    virtual RequestContextPtr<nx::Buffer> downloadChunk(
        const QnUuid& peerId,
        const QString& fileName,
        const nx::utils::Url& url,
        int chunkIndex,
        int chunkSize,
        qint64 fileSize) override;

private:
    TestPeerManager* m_peerManager;
    QHash<QnUuid, int> m_distances;
    RequestCounter m_requestCounter;
    QList<QnUuid> m_peerList;
};

} // namespace nx::vms::common::p2p::downloader
