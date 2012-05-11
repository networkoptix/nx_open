#include "tcp_listener.h"
#include "socket.h"
#include "utils/common/log.h"
#include "tcp_connection_processor.h"

// ------------------------ QnRtspListenerPrivate ---------------------------

class QnTcpListener::QnTcpListenerPrivate
{
public:
    QnTcpListenerPrivate() {
        serverSocket = 0;
        newPort = 0;
    }
    TCPServerSocket* serverSocket;
    QMap<TCPSocket*, CLLongRunnable*> connections;
    QByteArray authDigest;
    QMutex portMutex;
    int newPort;
    QHostAddress serverAddress;
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
        d->serverAddress = address;
        d->serverSocket = new TCPServerSocket(address.toString(), port, 5 ,true);
        start();
        cl_log.log("Server started at ", address.toString() + QString(":") + QString::number(port), cl_logINFO);
    }
    catch(const SocketException &e) {
        qCritical() << "Can't start TCP listener at address" << address << ":" << port << ". Reason: " << e.what();
    }
}

QnTcpListener::~QnTcpListener()
{
    Q_D(QnTcpListener);
    stop();

    d->serverSocket->close();
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

void QnTcpListener::removeAllConnections()
{
    Q_D(QnTcpListener);

    for (QMap<TCPSocket*, CLLongRunnable*>::iterator itr = d->connections.begin(); itr != d->connections.end(); ++itr)
    {
        CLLongRunnable* processor = itr.value();
        processor->pleaseStop();
    }

    for (QMap<TCPSocket*, CLLongRunnable*>::iterator itr = d->connections.begin(); itr != d->connections.end(); ++itr)
    {
        CLLongRunnable* processor = itr.value();
        delete processor;
    }
    d->connections.clear();
}

void QnTcpListener::updatePort(int newPort)
{
    Q_D(QnTcpListener);
    QMutexLocker lock(&d->portMutex);
    d->newPort = newPort;
}

void QnTcpListener::run()
{
    Q_D(QnTcpListener);
    if (!d->serverSocket)
        m_needStop = true;
    while (!m_needStop)
    {
        if (d->newPort)
        {
            QMutexLocker lock(&d->portMutex);
            removeAllConnections();
            delete d->serverSocket;
            d->serverSocket = new TCPServerSocket(d->serverAddress.toString(), d->newPort);
            d->newPort = 0;
        }

        TCPSocket* clientSocket = d->serverSocket->accept();
        if (clientSocket) {
            qDebug() << "New client connection from " << clientSocket->getPeerAddress() << ':' << clientSocket->getForeignPort();
            QnTCPConnectionProcessor* processor = createRequestProcessor(clientSocket, this);
            clientSocket->setReadTimeOut(processor->getSocketTimeout());
            clientSocket->setWriteTimeOut(processor->getSocketTimeout());

            d->connections[clientSocket] = processor;
            processor->start();
        }
        removeDisconnectedConnections();
    }
}
