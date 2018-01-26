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

namespace nx {
namespace vms {
namespace common {
namespace p2p {
namespace downloader {

class Storage;
class AbstractPeerManager;

class Worker: public QnLongRunnable, public ::std::enable_shared_from_this<Worker>
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
        validatingFileInformation,
        fileInformationValidated,
        requestingChecksums,
        downloadingChunks,
        finished,
        failed
    };
    Q_ENUM(State)

    Worker(
        const QString& fileName,
        Storage* storage,
        AbstractPeerManager* peerManager,
        QObject* parent = nullptr);
    virtual ~Worker();

    State state() const;

    int peersPerOperation() const;
    void setPeersPerOperation(int peersPerOperation);

    bool haveChunksToDownload();

    /**
     * Preferred peers could be used to hint the worker where to find the file.
     * This is used similarly to forced peers but does not limit the worker in selecting
     * other peers.
     */
    void setPreferredPeers(const QList<QnUuid>& preferredPeers);

signals:
    void finished(const QString& fileName);
    void failed(const QString& fileName);
    void chunkDownloadFailed(const QString& fileName);
    void stateChanged(State state);

private:
    void setState(State state);
    void doWork();
    void requestFileInformationInternal();
    void requestFileInformation();
    void validateFileInformation();
    void requestAvailableChunks();
    void handleFileInformationReply(
        bool success, rest::Handle handle, const FileInformation& fileInfo);
    void requestChecksums();
    void handleChecksumsReply(
        bool success, rest::Handle handle, const QVector<QByteArray>& checksums);
    void downloadNextChunk();
    void handleDownloadChunkReply(
        bool success, rest::Handle handle, int chunkIndex, const QByteArray& data);

    void cancelRequests();
    void cancelRequestsByType(State type);

    void finish();
    void fail();

    void updateLogTag();

    bool addAvailableChunksInfo(const QBitArray& chunks);
    QList<QnUuid> peersForChunk(int chunkIndex) const;

    void setShouldWait(bool value);
    void setShouldWaitForCb();
    virtual void run() override;
    virtual void pleaseStop() override;
    void pleaseStopUnsafe();
    bool haveChunksToDownloadUnsafe();
    virtual qint64 delayMs() const;

protected:
    FileInformation fileInformation() const;

    QList<QnUuid> peersWithInternetConnection() const;
    QList<QnUuid> selectPeersForOperation(
        int count = -1, QList<QnUuid> peers = QList<QnUuid>()) const;
    int selectNextChunk() const;

    bool isInternetAvailable(const QList<QnUuid>& peers = QList<QnUuid>()) const;
    bool isFileReadyForInternetDownload() const;
    QList<QnUuid> selectPeersForInternetDownload() const;
    bool needToFindBetterPeers() const;

    virtual void waitBeforeNextIteration(QnMutexLockerBase* lock);

    void increasePeerRank(const QnUuid& peerId, int value = 1);
    void decreasePeerRank(const QnUuid& peerId, int value = 1);

    void setPrintSelfPeerInLogs();
    static qint64 defaultStepDelay();
    AbstractPeerManager* peerManager() const;

private:
    struct RequestContext
    {
        QnUuid peerId;
        State type = State::failed;
        bool cancelled = false;

        RequestContext(const QnUuid& peerId, State type):
            peerId(peerId),
            type(type)
        {}

        RequestContext() = default;
    };

    bool m_shouldWait = false;
    mutable QnMutex m_mutex;
    QnWaitCondition m_waitCondition;
    Storage* m_storage = nullptr;
    AbstractPeerManager* m_peerManager = nullptr;
    const QString m_fileName;

    nx::utils::log::Tag m_logTag;
    bool m_printSelfPeerInLogs = false;

    State m_state = State::initial;

    bool m_started = false;
    utils::StandaloneTimerManager m_stepDelayTimer;
    QHash<rest::Handle, RequestContext> m_contextByHandle;

    QBitArray m_availableChunks;
    QBitArray m_downloadingChunks;

    int m_peersPerOperation = 1;
    struct PeerInformation
    {
        QBitArray downloadedChunks;
        int rank = 0;

        void increaseRank(int value = 1);
        void decreaseRank(int value = 1);
    };
    QHash<QnUuid, PeerInformation> m_peerInfoById;

    int m_subsequentChunksToDownload;
    bool m_usingInternet = false;
    bool m_fileInfoValidated = false;
};

} // namespace downloader
} // namespace p2p
} // namespace common
} // namespace vms
} // namespace nx
