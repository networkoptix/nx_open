#include "tcp_listener.h"
#include "socket.h"

static const int SOCK_TIMEOUT = 5000;

// ------------------------ QnRtspListenerPrivate ---------------------------

class QnTcpListener::QnTcpListenerPrivate
{
public:
    QnTcpListenerPrivate() {
        serverSocket = 0;
    }
    TCPServerSocket* serverSocket;
    QMap<TCPSocket*, CLLongRunnable*> connections;
};

// ------------------------ QnRtspListener ---------------------------

QnTcpListener::QnTcpListener(const QHostAddress& address, int port):
    d_ptr(new QnTcpListenerPrivate())
{
    Q_D(QnTcpListener);
    try {
        d->serverSocket = new TCPServerSocket(address.toString(), port);
        start();
        qDebug() << "RTSP server started at " << address << ":" << port;
    }
    catch(SocketException& e) {
        qWarning() << "Can't start RTSP listener at address" << address << ":" << port;
    }
}

QnTcpListener::~QnTcpListener()
{
    Q_D(QnTcpListener);
    d->serverSocket->close();
    stop();
    delete d->serverSocket;
    delete d_ptr;
}

void QnTcpListener::removeDisconnectedConnections()
{
    Q_D(QnTcpListener);
    for (QMap<TCPSocket*, CLLongRunnable*>::iterator itr = d->connections.begin(); itr != d->connections.end();)
    {
        CLLongRunnable* processor = itr.value();
        if (!processor->isRunning()) {
            delete processor;
            itr = d->connections.erase(itr);
        }
        else 
            ++itr;
    }
}

void QnTcpListener::run()
{
    Q_D(QnTcpListener);
    while (!m_needStop)
    {
        TCPSocket* clientSocket = d->serverSocket->accept();
        if (clientSocket) {
            qDebug() << "New client connection from " << clientSocket->getPeerAddress();
            clientSocket->setReadTimeOut(SOCK_TIMEOUT);
            clientSocket->setWriteTimeOut(SOCK_TIMEOUT);
            CLLongRunnable* processor = createRequestProcessor(clientSocket, this);
            d->connections[clientSocket] = processor;
            processor->start();
        }
        removeDisconnectedConnections();
    }
}

