#pragma once

#include <QtCore/QObject>
#include <QtCore/QBitArray>

#include <nx/utils/uuid.h>
#include <api/server_rest_connection_fwd.h>

#include "../file_information.h"

class QTimer;

namespace nx {
namespace vms {
namespace common {
namespace distributed_file_downloader {

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

    /*! Peers from which the worker will choose to perform the operation.
        All other peers from the peer manager will be ignored.
        This is used by updates manager to prevent the downloader from looking for files
        on peers which cannot contain an apropriate update file, e.g. ignore Linux servers
        when looking for updates for a Windows server. */
    QList<QnUuid> peers() const;
    void setPeers(const QList<QnUuid>& peers);

    /*! Preferred peers could be used to hint the worker where to find the file.
        This is used similarly to forced peers but does not limit the worker in selecting
        other peers.
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
    void requestChecksums();
    void downloadNextChunk();

    void cancelRequests();

    void finish();
    void fail();

    QString logMessage(const char* message) const;

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
    Storage* m_storage;
    AbstractPeerManager* m_peerManager;
    const QString m_fileName;

    bool m_printSelfPeerInLogs = false;

    State m_state = State::initial;

    bool m_started = false;
    QTimer* m_stepDelayTimer;
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
    QList<QnUuid> m_peers;

    int m_subsequentChunksToDownload = -1;
    bool m_usingInternet = false;
};

} // namespace distributed_file_downloader
} // namespace common
} // namespace vms
} // namespace nx
