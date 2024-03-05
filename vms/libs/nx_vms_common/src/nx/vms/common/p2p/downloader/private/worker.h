// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <thread>

#include <QtCore/QBitArray>
#include <QtCore/QElapsedTimer>

#include <api/server_rest_connection_fwd.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/utils/thread/long_runnable.h>

#include "../file_information.h"
#include "abstract_peer_manager.h"

namespace nx::vms::common::p2p::downloader {

using namespace std::chrono;

class Storage;

class NX_VMS_COMMON_API Worker: public QnLongRunnable
{
    Q_OBJECT

public:
    NX_REFLECTION_ENUM_CLASS_IN_CLASS(State,
        initial,
        requestingFileInformation,
        foundFileInformation,
        requestingAvailableChunks,
        foundAvailableChunks,
        requestingChecksums,
        downloadingChunks,
        finished,
        failed
    )

    struct Peer
    {
        nx::Uuid id;
        AbstractPeerManager* manager = nullptr;

        QString toString() const { return manager ? manager->peerString(id) : QString(); }

        bool isNull() const { return !manager && id.isNull(); }

        bool operator==(const Peer& other) const
        {
            return id == other.id && manager == other.manager;
        }

        friend size_t qHash(const Peer& peer)
        {
            return qHash(peer.id) + qHash(peer.manager);
        }
    };

    /**
     * Constructs a Worker
     * NOTE: we need to ensure that peerManager is alive for all lifetime of a Worker.
     */
    Worker(
        const QString& fileName,
        Storage* storage,
        const QList<AbstractPeerManager*>& peerManagers,
        const nx::Uuid& selfId);
    virtual ~Worker() override;

    State state() const;

    bool haveChunksToDownload();

    nx::log::Tag logTag() const;

    bool isStalled() const;

signals:
    void finished(const QString& fileName);
    void failed(const QString& fileName);
    void chunkDownloadFailed(const QString& fileName);
    void stateChanged(State state);
    void stalledChanged(bool stalled);

private:
    struct RequestHandle
    {
        AbstractPeerManager* peerManager;
        rest::Handle handle;

        QString toString() const
        {
            return QString("0x%1:%2").arg((size_t) peerManager, 0, 16).arg(handle);
        }

        bool operator==(const RequestHandle& other) const
        {
            return peerManager == other.peerManager && handle == other.handle;
        }

        friend size_t qHash(const RequestHandle& handle)
        {
            return qHash(handle.peerManager) + ::qHash(handle.handle);
        }
    };

    void setState(State state);
    void doWork();
    void requestFileInformationInternal();
    void requestFileInformation();
    void requestAvailableChunks();
    void handleFileInformationReply(
        const Peer& peer, const std::optional<FileInformation>& fileInfo);
    void requestChecksums();
    void handleChecksumsReply(
        const Peer& peer, const std::optional<QVector<QByteArray>>& checksums);
    void downloadChunks();
    void handleDownloadChunkReply(
        const Peer& peer,
        int chunkIndex,
        const std::optional<nx::Buffer>& data,
        bool decreaseRankOnFailure = true);

    void finish(State state = State::finished);

    int updateAvailableChunks();

    void sleep();
    void wake();
    virtual void run() override;
    virtual void pleaseStop() override;
    virtual std::chrono::milliseconds delay() const;

protected:
    FileInformation fileInformation() const;

    QSet<Peer> getPeersToCheckInfo() const;
    QList<Peer> getPeersToGetCheksums() const;
    Peer selectPeerForChunk(int chunk, const QSet<Peer>& busyPeers) const;

    void reviveBannedPeers();
    int selectNextChunk(const QSet<int>& ignoredChunks) const;

    bool needToFindBetterPeersForDownload() const;

    void increasePeerRank(const Peer& peer, int value = 1);
    void decreasePeerRank(const Peer& peer, int value = 1);

    QList<AbstractPeerManager*> peerManagers() const;

    void markActive();
    void checkStalled();
    void reDownload();

private:
    nx::Uuid m_selfId;
    Storage* m_storage = nullptr;
    QList<AbstractPeerManager*> m_peerManagers;
    const QString m_fileName;

    nx::log::Tag m_logTag;

    bool m_sleeping = false;
    std::mutex m_sleepMutex;
    std::condition_variable m_sleepCondition;

    State m_state = State::initial;

    bool m_started = false;

    QBitArray m_availableChunks;

    struct PeerInformation
    {
        static constexpr int kMaxAutoRank = 3;
        static constexpr int kMinAutoRank = 0;

        QBitArray downloadedChunks;
        int rank = kMaxAutoRank / 2;
        bool isInternet = false;
        QList<milliseconds> latestChunksDownloadTime;
        // The default value `0` here is intentional. When we find a new peer, we'll prefer it and
        // measure its chunk download time.
        milliseconds averageChunkDownloadTime{0};
        time_point<steady_clock> lastSuccessfulRequestTime{};

        void increaseRank(int value = 1);
        void decreaseRank(int value = 1);
        bool isBanned() const { return rank <= kMinAutoRank; }
        void recordChunkDownloadTime(milliseconds time);
        bool hasChunk(int chunk) const;
    };
    QHash<Peer, PeerInformation> m_peerInfoByPeer;

    bool m_retryingStep = false;
    int m_downloadRetriesCount = 0;
    int m_subsequentWriteErrors = 0;

    QElapsedTimer m_stallDetectionTimer;
    bool m_stalled = false;
};

} // namespace nx::vms::common::p2p::downloader
