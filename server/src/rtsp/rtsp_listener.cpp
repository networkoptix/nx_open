#include "rtsp_listener.h"
#include "rtsp_connection.h"
#include "network/socket.h"

static const int SOCK_TIMEOUT = 50;
// ------------------------ QnRtspListenerPrivate ---------------------------

struct QnRtspListener::QnRtspListenerPrivate
{
    TCPServerSocket* serverSocket;
    QMap<TCPSocket*, QnRtspConnectionProcessor*> connections;
};

// ------------------------ QnRtspListener ---------------------------

QnRtspListener::QnRtspListener(const QHostAddress& address, int port):
    d_ptr(new QnRtspListenerPrivate())
{
    Q_D(QnRtspListener);
    d->serverSocket = new TCPServerSocket(address.toString(), port);
    start();
    qDebug() << "RTSP server started at " << address << ":" << port;
    /*
    connect(&d->serverSocket, SIGNAL(newConnection()), this, SLOT(onNewConnection()));
    if (d->serverSocket.listen(address, port))
        qDebug() << "RTSP server started at " << address << ":" << port;
    else
        qWarning() << "Failed to start RTSP server at " << address << ":" << port;
    */
}

QnRtspListener::~QnRtspListener()
{
    Q_D(QnRtspListener);
    delete d->serverSocket;
    delete d_ptr;
}

void QnRtspListener::removeDisconnectedConnections()
{
    Q_D(QnRtspListener);
    for (QMap<TCPSocket*, QnRtspConnectionProcessor*>::iterator itr = d->connections.begin(); itr != d->connections.end();)
    {
        QnRtspConnectionProcessor* processor = itr.value();
        if (!processor->isRunning()) {
            delete processor;
            itr = d->connections.erase(itr);
        }
        else 
            ++itr;
    }
}

void QnRtspListener::run()
{
    Q_D(QnRtspListener);
    while (!m_needStop)
    {
        TCPSocket* clientSocket = d->serverSocket->accept();
        if (clientSocket) {
            qDebug() << "New client connection from " << clientSocket->getPeerAddress();
            clientSocket->setReadTimeOut(SOCK_TIMEOUT);
            clientSocket->setWriteTimeOut(SOCK_TIMEOUT);
            QnRtspConnectionProcessor* processor = new QnRtspConnectionProcessor(clientSocket);
            d->connections[clientSocket] = processor;
            processor->start();
        }
        removeDisconnectedConnections();
    }
}
