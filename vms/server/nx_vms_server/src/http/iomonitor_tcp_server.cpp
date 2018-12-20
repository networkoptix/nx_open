#include "iomonitor_tcp_server.h"

#include <QUrlQuery>
#include <QtCore/QCoreApplication>

#include "core/resource/camera_resource.h"
#include "core/resource_management/resource_pool.h"
#include "common/common_module.h"
#include <nx/network/http/custom_headers.h>
#include "network/tcp_connection_priv.h"
#include "nx/fusion/serialization/json.h"
#include <nx/fusion/model_functions.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>
#include <network/tcp_listener.h>
#include <api/helpers/camera_id_helper.h>
#include <nx/vms/server/resource/camera.h>

class QnIOMonitorConnectionProcessorPrivate: public QnTCPConnectionProcessorPrivate
{
public:
    QnMutex dataMutex;
    QnMutex waitMutex;
    QnWaitCondition waitCond;
    QnIOStateDataList dataToSend;
    QByteArray requestBuffer;

    std::deque<QByteArray> messagesToSend;
    QMap<QString, QnIOStateData> portState;
};

QnIOMonitorConnectionProcessor::QnIOMonitorConnectionProcessor(
    std::unique_ptr<nx::network::AbstractStreamSocket> socket,
    QnTcpListener* owner)
    :
    QnTCPConnectionProcessor(new QnIOMonitorConnectionProcessorPrivate, std::move(socket), owner)
{
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
        const QString cameraId = QUrlQuery(getDecodedUrl().query())
            .queryItemValue(Qn::PHYSICAL_ID_URL_QUERY_ITEM);

        const auto camera = nx::camera_id_helper::findCameraByFlexibleId(
            resourcePool(), cameraId).dynamicCast<nx::vms::server::resource::Camera>();
        if (!camera)
        {
            sendResponse(nx::network::http::StatusCode::notFound,
                "multipart/x-mixed-replace; boundary=ioboundary");
            return;
        }

        Qn::directConnect(camera.data(), &nx::vms::server::resource::Camera::initializedChanged,
            this, &QnIOMonitorConnectionProcessor::at_cameraInitDone);
        Qn::directConnect(camera.data(), &nx::vms::server::resource::Camera::inputPortStateChanged,
            this, &QnIOMonitorConnectionProcessor::at_cameraIOStateChanged);
        Qn::directConnect(camera.data(), &nx::vms::server::resource::Camera::outputPortStateChanged,
            this, &QnIOMonitorConnectionProcessor::at_cameraIOStateChanged);

        if (camera->getParentId() != commonModule()->moduleGUID())
        {
            sendResponse(nx::network::http::StatusCode::notFound,
                "multipart/x-mixed-replace; boundary=ioboundary");
            return;
        }
        else
        {
            sendResponse(
                nx::network::http::StatusCode::ok,
                "multipart/x-mixed-replace; boundary=ioboundary");
        }

        camera->inputPortListenerAttached();
        static const int REQUEST_BUFFER_SIZE = 1024;
        static const int KEEPALIVE_INTERVAL = 1000 * 5;
        d->requestBuffer.reserve(REQUEST_BUFFER_SIZE);
        d->socket->setRecvTimeout(KEEPALIVE_INTERVAL);
        d->socket->setNonBlockingMode(true);

        using namespace std::placeholders;
        d->socket->readSomeAsync(
            &d->requestBuffer,
            std::bind(&QnIOMonitorConnectionProcessor::onSomeBytesReadAsync,
                      this, d->socket.get(), _1, _2));

        addData(camera->ioPortStates());
        QnMutexLocker lock(&d->waitMutex);
        while (!needToStop()
               && d->socket->isConnected()
               && camera->getStatus() >= Qn::Online
               && camera->getParentId() == commonModule()->moduleGUID())
        {
            sendData();
            d->waitCond.wait(&d->waitMutex);
        }
        disconnect( camera.data(), nullptr, this, nullptr );
        lock.unlock();
        camera->inputPortListenerDetached();
        d->socket->pleaseStopSync();
    }
    d->socket->close();
}

void QnIOMonitorConnectionProcessor::at_cameraInitDone(const QnResourcePtr& resource)
{
    Q_D(QnIOMonitorConnectionProcessor);
    QnMutexLocker lock(&d->waitMutex);
    const auto camera = resource.dynamicCast<nx::vms::server::resource::Camera>();
    if (camera && camera->isInitialized())
    {
        addData(camera->ioPortStates());
        d->waitCond.wakeAll();
    }
}

void QnIOMonitorConnectionProcessor::onSomeBytesReadAsync(
    nx::network::AbstractStreamSocket* /*sock*/,
    SystemError::ErrorCode errorCode,
    size_t /*bytesRead*/)
{
    Q_D(QnIOMonitorConnectionProcessor);
    QnMutexLocker lock(&d->waitMutex); // just in case to prevent socket simultaneous calls while send / read async

    using namespace std::placeholders;
    d->requestBuffer.resize(0);
    if(errorCode == SystemError::timedOut)
    {
        d->socket->readSomeAsync(
            &d->requestBuffer,
            std::bind( &QnIOMonitorConnectionProcessor::onSomeBytesReadAsync, this, d->socket.get(), _1, _2 ) );
    }
    d->waitCond.wakeAll();
}

void QnIOMonitorConnectionProcessor::at_cameraIOStateChanged(
    const QnResourcePtr& /*resource*/,
    const QString& inputPortID,
    bool value,
    qint64 timestamp )
{
    Q_D(QnIOMonitorConnectionProcessor);
    QnMutexLocker lock(&d->waitMutex);
    addData(QnIOStateData(inputPortID, value, timestamp));
    d->waitCond.wakeAll();
}

void QnIOMonitorConnectionProcessor::sendData()
{
    Q_D(QnIOMonitorConnectionProcessor);

    QnMutexLocker lock(&d->dataMutex);

    d->response.messageBody = QJson::serialized<QnIOStateDataList>(d->dataToSend);
    d->dataToSend.clear();
    auto messageToSend = createResponse(nx::network::http::StatusCode::ok, "application/json", QByteArray(), "--ioboundary");

    d->socket->post(
        [this, messageToSend=std::move(messageToSend)]
        {
            Q_D(QnIOMonitorConnectionProcessor);
            d->messagesToSend.push_back(std::move(messageToSend));
            if (d->messagesToSend.size() > 1)
                return; //< previous IO in progress
            sendNextMessage();
        });
}

void QnIOMonitorConnectionProcessor::sendNextMessage()
{
    Q_D(QnIOMonitorConnectionProcessor);

    using namespace std::placeholders;
    d->socket->sendAsync(d->messagesToSend.front(),
        std::bind(&QnIOMonitorConnectionProcessor::onDataSent, this, _1, _2));
}

void QnIOMonitorConnectionProcessor::onDataSent(SystemError::ErrorCode errorCode, size_t bytesSent)
{
    Q_D(QnIOMonitorConnectionProcessor);

    d->messagesToSend.pop_front(); //< mark send done
    if (errorCode != SystemError::noError)
    {
        qWarning() << "QnIOMonitorConnectionProcessor::onDataSent write error. errCode=" << errorCode;
        pleaseStop();
        return;
    }

    if (!d->messagesToSend.empty())
        sendNextMessage();
}

void QnIOMonitorConnectionProcessor::addData(const QnIOStateDataList& value)
{
    Q_D(QnIOMonitorConnectionProcessor);
    QnMutexLocker lock(&d->dataMutex);
    for (const auto& ioState: value)
        addDataIfNeed(ioState);
}

void QnIOMonitorConnectionProcessor::addData(const QnIOStateData& value)
{
    Q_D(QnIOMonitorConnectionProcessor);
    QnMutexLocker lock(&d->dataMutex);
    addDataIfNeed(value);
}

void QnIOMonitorConnectionProcessor::addDataIfNeed(const QnIOStateData& value)
{
    Q_D(QnIOMonitorConnectionProcessor);
    auto& oldValue = d->portState[value.id];
    if (oldValue == value)
        return;
    oldValue = value;
    d->dataToSend.push_back(value);
}

void QnIOMonitorConnectionProcessor::pleaseStop()
{
    Q_D(QnIOMonitorConnectionProcessor);
    QnLongRunnable::pleaseStop();
    d->waitCond.wakeAll();
}
