#pragma once

#include <boost/optional.hpp>

#include <QtCore/QObject>
#include <QtCore/QBitArray>

#include <nx/utils/log/log_level.h>
#include <nx/utils/uuid.h>
#include <api/server_rest_connection_fwd.h>

#include "../file_information.h"
#include "abstract_peer_manager.h"

class QTimer;

namespace nx {
namespace vms {
namespace common {
namespace p2p {
namespace downloader {

class Storage;
class AbstractPeerManager;

class Worker: public QObject
{
    Q_OBJECT

    using base_type = QObject;

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

    Worker(
        const QString& fileName,
        Storage* storage,
        AbstractPeerManager* peerManager,
        QObject* parent = nullptr);
    virtual ~Worker();

    void start();

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
    void stateChanged(State state);

private:
    void setState(State state);
    void nextStep();
    void requestFileInformationInternal();
    void requestFileInformation();
    void requestAvailableChunks();
    void handleFileInformationReply(
        bool success, rest::Handle handle, const FileInformation& fileInfo);
    void requestChecksums();
    void handleChecksumsReply(
        bool success, rest::Handle handle, const QVector<QByteArray>& checksums);
    void downloadNextChunk();
    void handleDownloadChunkReply(
        AbstractPeerManager::ChunkDownloadResult chunkDownloadResult,
        rest::Handle handle,
        int chunkIndex,
        const QByteArray& data);

    void cancelRequests();

    void finish();
    void fail();

    void updateLogTag();

    bool addAvailableChunksInfo(const QBitArray& chunks);
    QList<QnUuid> peersForChunk(int chunkIndex) const;

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

    virtual void waitForNextStep(int delay = -1);

    void increasePeerRank(const QnUuid& peerId, int value = 1);
    void decreasePeerRank(const QnUuid& peerId, int value = 1);

    void setPrintSelfPeerInLogs();
    static qint64 defaultStepDelay();
    AbstractPeerManager* peerManager() const;

private:
    Storage* m_storage = nullptr;
    AbstractPeerManager* m_peerManager = nullptr;
    const QString m_fileName;

    nx::utils::log::Tag m_logTag;
    bool m_printSelfPeerInLogs = false;

    State m_state = State::initial;

    bool m_started = false;
    QTimer* m_stepDelayTimer = nullptr;
    QHash<rest::Handle, QnUuid> m_peerByRequestHandle;

    QBitArray m_availableChunks;

    int m_peersPerOperation = 1;
    struct PeerInformation
    {
        QBitArray downloadedChunks;
        int rank = 0;

        void increaseRank(int value = 1);
        void decreaseRank(int value = 1);
    };
    QHash<QnUuid, PeerInformation> m_peerInfoById;

    boost::optional<int> m_subsequentChunksToDownload;
    bool m_usingInternet = false;
};

} // namespace downloader
} // namespace p2p
} // namespace common
} // namespace vms
} // namespace nx
