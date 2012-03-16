#include "test_camera_processor.h"
#include "utils/network/tcp_connection_priv.h"
#include "test_camera.h"
#include "camera_pool.h"

class QnTestCameraProcessor::QnTestCameraProcessorPrivate: public QnTCPConnectionProcessor::QnTCPConnectionProcessorPrivate
{
public:
    //enum State {State_Stopped, State_Paused, State_Playing, State_Rewind};

    QnTestCameraProcessorPrivate():
      QnTCPConnectionProcessor::QnTCPConnectionProcessorPrivate()
      {

      }
};

QnTestCameraProcessor::QnTestCameraProcessor(TCPSocket* socket, QnTcpListener* owner):
    QnTCPConnectionProcessor(socket, owner)
{
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
        QByteArray macAddress((const char*) buffer, readed);
        if (!macAddress.isEmpty() && macAddress[0] == '/')
            macAddress = macAddress.mid(1);
        QnTestCamera* camera = QnCameraPool::instance()->findCamera(macAddress);
        if (camera == 0)
        {
            qDebug() << "No camera found with MAC address " << macAddress;
            return;
        }
        camera->startStreaming(d->socket);
    }
}
