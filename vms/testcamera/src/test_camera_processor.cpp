#include "test_camera_processor.h"
#include <network/tcp_connection_priv.h>
#include "test_camera.h"
#include "camera_pool.h"

class QnTestCameraProcessorPrivate: public QnTCPConnectionProcessorPrivate
{
public:
    using QnTCPConnectionProcessorPrivate::QnTCPConnectionProcessorPrivate;
};

QnTestCameraProcessor::QnTestCameraProcessor(
    std::unique_ptr<nx::network::AbstractStreamSocket> socket,
    QnTcpListener* owner,
    bool noSecondaryStream,
    int fpsPrimary,
    int fpsSecondary)
    :
    QnTCPConnectionProcessor(std::move(socket), owner),
    m_noSecondaryStream(noSecondaryStream),
    m_fpsPrimary(fpsPrimary),
    m_fpsSecondary(fpsSecondary)
{
}

QnTestCameraProcessor::~QnTestCameraProcessor()
{
    stop();
}

void QnTestCameraProcessor::run()
{
    Q_D(QnTestCameraProcessor);
    quint8 buffer[128];
    if (m_needStop || !d->socket->isConnected())
        return;

    const int bytesRead = d->socket->recv(buffer, sizeof(buffer));
    if (bytesRead == 0)
    {
        qDebug() << "No MAC address specified";
        return;
    }
    QByteArray cameraUrl((const char*) buffer, bytesRead);
    if (!cameraUrl.isEmpty() && cameraUrl[0] == '/')
        cameraUrl = cameraUrl.mid(1);

    const int pos = cameraUrl.indexOf('?');
    const QByteArray macAddress = cameraUrl.left(pos);
    const QList<QByteArray> params = cameraUrl.mid(pos+1).split('&');

    QnTestCamera* const camera = QnCameraPool::instance()->findCamera(macAddress);
    if (camera == nullptr)
    {
        qDebug() << "No camera found with MAC address " << macAddress;
        return;
    }

    // Try to receive the values from the Client.
    bool isSecondary = false;
    int fps = 10;
    for (int i = 0; i < params.size(); ++ i)
    {
        QList<QByteArray> paramVal = params[i].split('=');
        if (paramVal[0] == "primary" && paramVal.size() == 2)
        {
            if (paramVal[1] != "1")
                isSecondary = true;
        }
        else if (paramVal[0] == "fps" && paramVal.size() == 2)
        {
            fps = paramVal[1].toInt();
        }
    }

    if (isSecondary && m_noSecondaryStream)
        return;

    const int overridingFps = isSecondary ? m_fpsSecondary : m_fpsPrimary;
    if (overridingFps != -1)
        fps = overridingFps;

    qDebug() << "Start" << (isSecondary ? "secondary" : "primary") << "stream at" << fps << "fps";
    camera->startStreaming(d->socket.get(), isSecondary, fps, commonModule());
}
