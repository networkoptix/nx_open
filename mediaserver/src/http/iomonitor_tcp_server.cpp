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
        QString uniqueId = QUrlQuery(getDecodedUrl().query()).queryItemValue(lit("id"));
        QnSecurityCamResourcePtr camera = qnResPool->getResourceByUniqueId<QnSecurityCamResource>(uniqueId);
        if (!camera) {
            sendResponse(CODE_NOT_FOUND, "multipart/x-mixed-replace; boundary=ioboundary");
            return;
        }
        
        sendResponse(CODE_OK, "multipart/x-mixed-replace; boundary=ioboundary");

        connect(camera.data(), &QnSecurityCamResource::cameraInput, this, &QnIOMonitorConnectionProcessor::at_cameraInput);
        connect(camera.data(), &QnSecurityCamResource::cameraOutput, this, &QnIOMonitorConnectionProcessor::at_cameraOutput);

        while (d->socket->isConnected()) 
        {
            QMutexLocker lock(&d->waitMutex);
            while (d->dataToSend.empty() && d->socket->isConnected())
                d->waitCond.wait(&d->waitMutex);
            sendMultipartData();
        }
    }
}

void QnIOMonitorConnectionProcessor::at_cameraInput(
    const QnResourcePtr& resource,
    const QString& inputPortID,
    bool value,
    qint64 timestamp )
{
    // todo: implement me
}

void QnIOMonitorConnectionProcessor::at_cameraOutput(
    const QnResourcePtr& resource,
    const QString& inputPortID,
    bool value,
    qint64 timestamp )
{
    // todo: implement me
}

void QnIOMonitorConnectionProcessor::sendMultipartData()
{
    Q_D(QnIOMonitorConnectionProcessor);
    d->response.messageBody = QJson::serialized<QnIOStateDataList>(d->dataToSend);
    d->dataToSend.clear();
    sendResponse(CODE_OK, "application/json", QByteArray(), "--ioboundary");

        //lit("--ioboundary\r\nContent-Type: application/json\r\n\r\n%1\r\n").arg(payloadData);
    //d->response.clear
    //QMutexLocker lock(&d->sockMutex);
    //sendData(response.data(), response.size());
}

void QnIOMonitorConnectionProcessor::pleaseStop()
{
    Q_D(QnIOMonitorConnectionProcessor);
    QnTCPConnectionProcessor::pleaseStop();
    d->waitCond.wakeAll();
}
