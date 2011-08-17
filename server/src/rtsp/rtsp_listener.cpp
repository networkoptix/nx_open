#include <QTcpServer>

#include "rtsp_listener.h"
#include "rtsp_connection.h"

// ------------------------ QnRtspListenerPrivate ---------------------------

struct QnRtspListener::QnRtspListenerPrivate
{
    QTcpServer serverSocket;
    QMap<QTcpSocket*, QnRtspConnectionProcessor*> connections;
};

// ------------------------ QnRtspListener ---------------------------

QnRtspListener::QnRtspListener(const QHostAddress& address, int port):d_ptr(new QnRtspListenerPrivate())
{
    Q_D(QnRtspListener);
    connect(&d->serverSocket, SIGNAL(newConnection()), this, SLOT(onNewConnection()));
    if (d->serverSocket.listen(address, port))
        qDebug() << "RTSP server started at " << address << ":" << port;
    else
        qWarning() << "Failed to start RTSP server at " << address << ":" << port;
}

QnRtspListener::~QnRtspListener()
{
    delete d_ptr;
}

void QnRtspListener::onNewConnection()
{
    Q_D(QnRtspListener);
    QTcpSocket* clientSocket = d->serverSocket.nextPendingConnection();
    d->connections[clientSocket] = new QnRtspConnectionProcessor(clientSocket);
    qDebug() << "New client connection from " << clientSocket->peerAddress();
}
