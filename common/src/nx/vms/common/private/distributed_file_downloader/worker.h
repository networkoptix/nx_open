#pragma once

#include <QtCore/QObject>

#include <nx/utils/uuid.h>
#include <api/server_rest_connection_fwd.h>

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
    Worker(
        const QString& fileName,
        Storage* storage,
        AbstractPeerManager* peerManager,
        QObject* parent = nullptr);
    virtual ~Worker();

    void start();

signals:
    void finished(const QString& fileName);

private:
    void nextStep();
    void findFileInformation();
    void findChunksInformation();
    void downloadNextChunk();

    void cancelRequests();

    QString logMessage(const char* message) const;

protected:
    virtual void waitForNextStep(int delay = -1);

private:
    Storage* m_storage;
    AbstractPeerManager* m_peerManager;
    const QString m_fileName;

    bool m_started = false;
    QTimer* m_stepDelayTimer;
    QHash<rest::Handle, QnUuid> m_peerByRequestHandle;
};

} // namespace distributed_file_downloader
} // namespace common
} // namespace vms
} // namespace nx
