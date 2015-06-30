#include <QUrlQuery>
#include <QWaitCondition>

#include "iomonitor_tcp_server.h"
#include <QtCore/QCoreApplication>
#include "utils/network/tcp_connection_priv.h"
#include "core/resource/camera_resource.h"
#include "core/resource_management/resource_pool.h"
#include "utils/serialization/json.h"
#include <utils/common/model_functions.h>

class QnIOMonitorConnectionProcessorPrivate: public QnTCPConnectionProcessorPrivate
{
public:
    QMutex waitMutex;
    QWaitCondition waitCond;
    QnIOStateDataList dataToSend;
    QByteArray requestBuffer;
};

QnIOMonitorConnectionProcessor::QnIOMonitorConnectionProcessor(QSharedPointer<AbstractStreamSocket> socket, QnTcpListener* owner):
    QnTCPConnectionProcessor(new QnIOMonitorConnectionProcessorPrivate, socket)
{
}

QnIOMonitorConnectionProcessor::~QnIOMonitorConnectionProcessor()
{
    stop();
}

void QnIOMonitorConnectionProcessor::run()
{
    Q_D(QnIOMonitorConnectionProcessor);
    initSystemThreadId();

    bool ready = true;
    if (d->clientRequest.isEmpty())
        ready = readRequest();

    if (ready)
    {
        parseRequest();
        QString uniqueId = QUrlQuery(getDecodedUrl().query()).queryItemValue(lit("physicalId"));
        QnSecurityCamResourcePtr camera = qnResPool->getResourceByUniqueId<QnSecurityCamResource>(uniqueId);
        if (!camera) {
            sendResponse(CODE_NOT_FOUND, "multipart/x-mixed-replace; boundary=ioboundary");
            return;
        }
        
        sendResponse(CODE_OK, "multipart/x-mixed-replace; boundary=ioboundary");

        connect(camera.data(), &QnSecurityCamResource::cameraInput, this, &QnIOMonitorConnectionProcessor::at_cameraInput, Qt::DirectConnection);
        connect(camera.data(), &QnSecurityCamResource::cameraOutput, this, &QnIOMonitorConnectionProcessor::at_cameraOutput, Qt::DirectConnection);

        static const int KEEPALIVE_INTERVAL = 1000 * 5;

        using namespace std::placeholders;
        static const int REQUEST_BUFFER_SIZE = 1024;
        d->requestBuffer.reserve(REQUEST_BUFFER_SIZE);
        d->socket->setRecvTimeout(KEEPALIVE_INTERVAL);
        d->socket->readSomeAsync( &d->requestBuffer, std::bind( &QnIOMonitorConnectionProcessor::onSomeBytesReadAsync, this, d->socket.data(), _1, _2 ) );

        auto iostates = camera->ioStates();
        QMutexLocker lock(&d->waitMutex);
        d->dataToSend = std::move(iostates);
        sendMultipartData(lock);
        while (d->socket->isConnected()) 
        {
            d->waitCond.wait(&d->waitMutex);
            if (d->socket->isConnected())
                sendMultipartData(lock);
        }
        d->socket->terminateAsyncIO(true);
    }
}

void QnIOMonitorConnectionProcessor::onSomeBytesReadAsync( AbstractSocket* sock, SystemError::ErrorCode errorCode, size_t bytesRead )
{
    Q_D(QnIOMonitorConnectionProcessor);
    QMutexLocker lock(&d->waitMutex);

    using namespace std::placeholders;
    d->requestBuffer.resize(0);
    if(errorCode == SystemError::timedOut)
        d->socket->readSomeAsync( &d->requestBuffer, std::bind( &QnIOMonitorConnectionProcessor::onSomeBytesReadAsync, this, d->socket.data(), _1, _2 ) );
    d->waitCond.wakeAll();
}

void QnIOMonitorConnectionProcessor::at_cameraInput(
    const QnResourcePtr& resource,
    const QString& inputPortID,
    bool value,
    qint64 timestamp )
{
    Q_D(QnIOMonitorConnectionProcessor);
    QMutexLocker lock(&d->waitMutex);
    d->dataToSend.push_back(QnIOStateData(inputPortID, value, timestamp));
    d->waitCond.wakeAll();
}

void QnIOMonitorConnectionProcessor::at_cameraOutput(
    const QnResourcePtr& resource,
    const QString& inputPortID,
    bool value,
    qint64 timestamp )
{
    Q_D(QnIOMonitorConnectionProcessor);
    QMutexLocker lock(&d->waitMutex);
    d->dataToSend.push_back(QnIOStateData(inputPortID, value, timestamp));
    d->waitCond.wakeAll();
}

void QnIOMonitorConnectionProcessor::sendMultipartData(QMutexLocker& lock)
{
    Q_D(QnIOMonitorConnectionProcessor);
    d->response.messageBody = QJson::serialized<QnIOStateDataList>(d->dataToSend);
    d->dataToSend.clear();
    lock.unlock();
    sendResponse(CODE_OK, "application/json", QByteArray(), "--ioboundary");
    lock.relock();
}

void QnIOMonitorConnectionProcessor::pleaseStop()
{
    Q_D(QnIOMonitorConnectionProcessor);
    QnTCPConnectionProcessor::pleaseStop();
    d->waitCond.wakeAll();
}
