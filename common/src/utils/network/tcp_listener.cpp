#include "tcp_listener.h"
#include "socket.h"
#include "utils/common/log.h"

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
    QByteArray authDigest;
};

// ------------------------ QnRtspListener ---------------------------

bool QnTcpListener::authenticate(const QHttpRequestHeader& headers, QHttpResponseHeader& responseHeaders) const
{
    Q_D(const QnTcpListener);
    if (d->authDigest.isEmpty())
        return true;
    QList<QByteArray> data = headers.value("Authorization").toUtf8().split(' ');
    bool rez = false;
    if (data[0].toLower() == "basic" && data.size() > 1)
        rez = data[1] == d->authDigest;
    if (!rez) {
        responseHeaders.addValue("WWW-Authenticate", "Basic realm=\"Secure Area\"");
    }
    return rez;
}

void QnTcpListener::setAuth(const QByteArray& userName, const QByteArray& password)
{
    Q_D(QnTcpListener);
    QByteArray digest = userName + QByteArray(":") + password;
    d->authDigest = digest.toBase64();
}


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
        qWarning() << "Can't start TCP listener at address" << address << ":" << port;
        cl_log.log("Can't start TCP listener at address ", address.toString() + ":" + QString::number(port), cl_logERROR);
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
