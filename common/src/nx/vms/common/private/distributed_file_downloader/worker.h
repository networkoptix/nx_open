#pragma once

#include <QtCore/QObject>
#include <QtCore/QBitArray>

#include <nx/utils/uuid.h>
#include <api/server_rest_connection_fwd.h>

#include "../../distributed_file_downloader.h"

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

signals:
    void finished(const QString& fileName);
    void failed(const QString& fileName);

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
    DownloaderFileInformation fileInformation() const;

    QList<QnUuid> selectPeersForOperation(
        int count = -1, const QList<QnUuid>& referencePeers = QList<QnUuid>());
    int selectNextChunk() const;

    virtual void waitForNextStep(int delay = -1);

private:
    Storage* m_storage;
    AbstractPeerManager* m_peerManager;
    const QString m_fileName;

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
};

} // namespace distributed_file_downloader
} // namespace common
} // namespace vms
} // namespace nx
