
#include "iomonitor_tcp_server.h"

#include <QUrlQuery>
#include <QtCore/QCoreApplication>

#include "core/resource/camera_resource.h"
#include "core/resource_management/resource_pool.h"
#include "common/common_module.h"
#include <http/custom_headers.h>
#include "network/tcp_connection_priv.h"
#include "utils/serialization/json.h"
#include <utils/common/model_functions.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>

class QnIOMonitorConnectionProcessorPrivate: public QnTCPConnectionProcessorPrivate
{
public:
    QnMutex dataMutex;
    QnMutex waitMutex;
    QnWaitCondition waitCond;
    QnIOStateDataList dataToSend;
    QByteArray requestBuffer;
};

QnIOMonitorConnectionProcessor::QnIOMonitorConnectionProcessor(QSharedPointer<AbstractStreamSocket> socket, QnTcpListener* owner):
    QnTCPConnectionProcessor(new QnIOMonitorConnectionProcessorPrivate, socket)
{
    QN_UNUSED(owner);
}

QnIOMonitorConnectionProcessor::~QnIOMonitorConnectionProcessor()
{
    directDisconnectAll();

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
        QString uniqueId = QUrlQuery(getDecodedUrl().query()).queryItemValue(Qn::CAMERA_UNIQUE_ID_HEADER_NAME);
        QnSecurityCamResourcePtr camera = qnResPool->getResourceByUniqueId<QnSecurityCamResource>(uniqueId);
        if (!camera) {
            sendResponse(CODE_NOT_FOUND, "multipart/x-mixed-replace; boundary=ioboundary");
            return;
        }
        Qn::directConnect(camera.data(), &QnSecurityCamResource::initializedChanged, this, &QnIOMonitorConnectionProcessor::at_cameraInitDone);
        Qn::directConnect(camera.data(), &QnSecurityCamResource::cameraInput, this, &QnIOMonitorConnectionProcessor::at_cameraIOStateChanged);
        Qn::directConnect(camera.data(), &QnSecurityCamResource::cameraOutput, this, &QnIOMonitorConnectionProcessor::at_cameraIOStateChanged);

        if (camera->getParentId() != qnCommon->moduleGUID()) {
            sendResponse(CODE_NOT_FOUND, "multipart/x-mixed-replace; boundary=ioboundary");
            return;
        }
        else {
            sendResponse(CODE_OK, "multipart/x-mixed-replace; boundary=ioboundary");
        }
        camera->inputPortListenerAttached();
        static const int REQUEST_BUFFER_SIZE = 1024;
        static const int KEEPALIVE_INTERVAL = 1000 * 5;
        d->requestBuffer.reserve(REQUEST_BUFFER_SIZE);
        d->socket->setRecvTimeout(KEEPALIVE_INTERVAL);

        using namespace std::placeholders;
        d->socket->readSomeAsync( &d->requestBuffer, std::bind( &QnIOMonitorConnectionProcessor::onSomeBytesReadAsync, this, d->socket.data(), _1, _2 ) );

        setData(camera->ioStates());
        QnMutexLocker lock(&d->waitMutex);
        while (d->socket->isConnected() && camera->getStatus() >= Qn::Online && camera->getParentId() == qnCommon->moduleGUID()) 
        {
            sendMultipartData();
            d->waitCond.wait(&d->waitMutex);
        }
        disconnect( camera.data(), nullptr, this, nullptr );
        lock.unlock();
        camera->inputPortListenerDetached();
        d->socket->terminateAsyncIO(true);
    }
}

void QnIOMonitorConnectionProcessor::at_cameraInitDone(const QnResourcePtr &resource)
{
    Q_D(QnIOMonitorConnectionProcessor);
    QnMutexLocker lock(&d->waitMutex);
    QnSecurityCamResourcePtr camera = resource.dynamicCast<QnSecurityCamResource>();
    if (camera && camera->isInitialized()) {
        setData(camera->ioStates());
        d->waitCond.wakeAll();
    }
}

void QnIOMonitorConnectionProcessor::onSomeBytesReadAsync( AbstractSocket* sock, SystemError::ErrorCode errorCode, size_t bytesRead )
{
    QN_UNUSED(sock, bytesRead);

    Q_D(QnIOMonitorConnectionProcessor);
    QnMutexLocker lock(&d->waitMutex); // just in case to prevent socket simultaneous calls while send / read async

    using namespace std::placeholders;
    d->requestBuffer.resize(0);
    if(errorCode == SystemError::timedOut)
        d->socket->readSomeAsync( &d->requestBuffer, std::bind( &QnIOMonitorConnectionProcessor::onSomeBytesReadAsync, this, d->socket.data(), _1, _2 ) );
    d->waitCond.wakeAll();
}

void QnIOMonitorConnectionProcessor::at_cameraIOStateChanged(
    const QnResourcePtr& resource,
    const QString& inputPortID,
    bool value,
    qint64 timestamp )
{
    QN_UNUSED(resource);

    Q_D(QnIOMonitorConnectionProcessor);
    QnMutexLocker lock(&d->waitMutex);
    addData(QnIOStateData(inputPortID, value, timestamp));
    d->waitCond.wakeAll();
}

void QnIOMonitorConnectionProcessor::sendMultipartData()
{
    Q_D(QnIOMonitorConnectionProcessor);
    {
        QnMutexLocker lock(&d->dataMutex);
        d->response.messageBody = QJson::serialized<QnIOStateDataList>(d->dataToSend);
        d->dataToSend.clear();
    }
    sendResponse(CODE_OK, "application/json", QByteArray(), "--ioboundary");
}

void QnIOMonitorConnectionProcessor::setData(QnIOStateDataList&& value)
{
    Q_D(QnIOMonitorConnectionProcessor);
    QnMutexLocker lock(&d->dataMutex);
    d->dataToSend = value;
}

void QnIOMonitorConnectionProcessor::addData(QnIOStateData&& value)
{
    Q_D(QnIOMonitorConnectionProcessor);
    QnMutexLocker lock(&d->dataMutex);
    d->dataToSend.push_back(value);
}

void QnIOMonitorConnectionProcessor::pleaseStop()
{
    Q_D(QnIOMonitorConnectionProcessor);
    QnTCPConnectionProcessor::pleaseStop();
    d->waitCond.wakeAll();
}
