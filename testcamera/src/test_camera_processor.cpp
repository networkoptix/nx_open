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
    const QSharedPointer<AbstractStreamSocket>& socket,
    QnTcpListener* owner,
    bool noSecondaryStream,
    int fps)
    :
    QnTCPConnectionProcessor(socket, owner),
    m_noSecondaryStream(noSecondaryStream),
    m_fps(fps)
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
    if (!m_needStop && d->socket->isConnected())
    {
        int readed = d->socket->recv(buffer, sizeof(buffer));
        if (readed == 0)
        {
            qDebug() << "No MAC address specified";
            return;
        }
        QByteArray cameraUrl((const char*) buffer, readed);
        if (!cameraUrl.isEmpty() && cameraUrl[0] == '/')
            cameraUrl = cameraUrl.mid(1);

        int pos = cameraUrl.indexOf('?');
        QByteArray macAddress = cameraUrl.left(pos);
        QList<QByteArray> params = cameraUrl.mid(pos+1).split('&');

        QnTestCamera* camera = QnCameraPool::instance()->findCamera(macAddress);
        if (camera == 0)
        {
            qDebug() << "No camera found with MAC address " << macAddress;
            return;
        }
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

        if (m_fps != -1)
            fps = m_fps;

        if (!isSecondary || !m_noSecondaryStream)
            camera->startStreaming(d->socket.data(), isSecondary, fps);
    }
}
