#pragma once

#include <memory>
#include <boost/optional.hpp>

#include <QtCore/QObject>
#include <QtCore/QBitArray>

#include <nx/utils/log/log_level.h>
#include <nx/utils/uuid.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>
#include <nx/utils/thread/long_runnable.h>
#include <nx/utils/timer_manager.h>
#include <api/server_rest_connection_fwd.h>

#include "../file_information.h"
#include "abstract_peer_manager.h"

namespace nx::vms::common::p2p::downloader {

class Storage;
class AbstractPeerManager;

class Worker: public QnLongRunnable
{
    Q_OBJECT

public:
    enum class State
    {
        initial,
        requestingFileInformation,
        foundFileInformation,
        requestingAvailableChunks,
        foundAvailableChunks,
        requestingChecksums,
        downloadingChunks,
        finished,
        failed
    };
    Q_ENUM(State)

    struct Peer
    {
        QnUuid id;
        AbstractPeerManager* manager = nullptr;

        QString toString() const { return manager->peerString(id); }

        bool isNull() const { return id.isNull(); }

        bool operator==(const Peer& other) const
        {
            return id == other.id && manager == other.manager;
        }

        friend uint qHash(const Peer& peer)
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
        const QnUuid& selfId);
    virtual ~Worker() override;

    State state() const;

    int peersPerOperation() const;
    void setPeersPerOperation(int peersPerOperation);

    bool haveChunksToDownload();

signals:
    void finished(const QString& fileName);
    void failed(const QString& fileName);
    void chunkDownloadFailed(const QString& fileName);
    void stateChanged(State state);

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

        friend uint qHash(const RequestHandle& handle)
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
        bool success, RequestHandle handle, const FileInformation& fileInfo);
    void requestChecksums();
    void handleChecksumsReply(
        bool success, RequestHandle handle, const QVector<QByteArray>& checksums);
    void downloadNextChunk();
    void handleDownloadChunkReply(
        bool success, RequestHandle handle, int chunkIndex, const QByteArray& data);

    void cancelRequests();
    void cancelRequestsByType(State type);
    bool hasPendingRequestsByType(State type) const;

    void finish(State state = State::finished);

    bool addAvailableChunksInfo(const QBitArray& chunks);
    QList<Peer> peersForChunk(int chunkIndex) const;

    void setShouldWait(bool value);
    void setShouldWaitForAsyncOperationCompletion();
    virtual void run() override;
    virtual void pleaseStop() override;
    void pleaseStopUnsafe();
    bool haveChunksToDownloadUnsafe();
    virtual qint64 delayMs() const;
    bool hasNotDownloadingChunks() const;

protected:
    FileInformation fileInformation() const;

    QList<Peer> selectPeersForOperation(AbstractPeerManager::Capability capability,
        int count = -1,
        const QList<Peer>& allowedPeers = {}) const;
    QList<QnUuid> selectPeersForOperation(
        AbstractPeerManager* peerManagers, int count = -1, QList<QnUuid> peers = {}) const;
    void revivePeersWithMinimalRank();
    int selectNextChunk() const;

    bool needToFindBetterPeers() const;

    virtual void waitBeforeNextIteration(QnMutexLockerBase* lock);

    void increasePeerRank(const Peer& peer, int value = 1);
    void decreasePeerRank(const Peer& peer, int value = 1);

    static qint64 defaultStepDelay();
    QList<AbstractPeerManager*> peerManagers() const;

private:
    struct RequestContext
    {
        Peer peer;
        State type = State::failed;

        RequestContext(const Peer& peerId, State type):
            peer(peerId),
            type(type)
        {}

        RequestContext() = default;
    };

    QnUuid m_selfId;
    bool m_shouldWait = false;
    mutable QnMutex m_mutex;
    QnWaitCondition m_waitCondition;
    Storage* m_storage = nullptr;
    QList<AbstractPeerManager*> m_peerManagers;
    const QString m_fileName;

    nx::utils::log::Tag m_logTag;

    State m_state = State::initial;

    bool m_started = false;
    utils::StandaloneTimerManager m_stepDelayTimer;

    QHash<RequestHandle, RequestContext> m_contextByHandle;

    QBitArray m_availableChunks;
    QBitArray m_downloadingChunks;

    int m_peersPerOperation = 1;
    struct PeerInformation
    {
        QBitArray downloadedChunks;
        int rank = 0;
        bool isInternet = false;

        void increaseRank(int value = 1);
        void decreaseRank(int value = 1);
    };
    QHash<Peer, PeerInformation> m_peerInfoByPeer;

    int m_subsequentChunksToDownload;
    bool m_usingInternet = false;
    bool m_fileInfoValidated = false;
};

} // namespace nx::vms::common::p2p::downloader
