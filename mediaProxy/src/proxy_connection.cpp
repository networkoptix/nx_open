#include <QDebug>

#include "proxy_connection.h"
#include "utils/network/socket.h"
#include "utils/network/tcp_connection_priv.h"

class QnTcpListener;
static const int IO_TIMEOUT = 1000 * 1000;

// ----------------------------- QnProxyConnectionProcessorPrivate ----------------------------

class QnProxyConnectionProcessor::QnProxyConnectionProcessorPrivate: public QnTCPConnectionProcessor::QnTCPConnectionProcessorPrivate
{
public:
    QnProxyConnectionProcessorPrivate():
        QnTCPConnectionProcessor::QnTCPConnectionProcessorPrivate(),
        dstSocket(0)
    {
    }
    virtual ~QnProxyConnectionProcessorPrivate()
    {
        delete dstSocket;
    }

    TCPSocket* dstSocket;
};

// ----------------------------- QnProxyConnectionProcessor ----------------------------

QnProxyConnectionProcessor::QnProxyConnectionProcessor(TCPSocket* socket, QnTcpListener* _owner):
    QnTCPConnectionProcessor(new QnProxyConnectionProcessorPrivate, socket, _owner)
{
    Q_D(QnProxyConnectionProcessor);
    d->dstSocket = new TCPSocket();
    d->dstSocket->setReadTimeOut(IO_TIMEOUT);
    d->dstSocket->setWriteTimeOut(IO_TIMEOUT);
}

QnProxyConnectionProcessor::~QnProxyConnectionProcessor()
{
    stop();
}

int QnProxyConnectionProcessor::getDefaultPortByProtocol(const QString& protocol)
{
    if (protocol == "http")
        return 80;
    else if (protocol == "https")
        return 443;
    else if (protocol == "rtsp")
        return 554;
    else 
        return -1;
}

bool QnProxyConnectionProcessor::doProxyData(fd_set* read_set, TCPSocket* srcSocket, TCPSocket* dstSocket, char* buffer, int bufferSize)
{
    if(FD_ISSET(srcSocket->handle(), read_set))
    {
        int readed = srcSocket->recv(buffer, bufferSize);
        if (readed < 1)
            return false;
        dstSocket->send(buffer, readed);
    }
    return true;
}

void QnProxyConnectionProcessor::run()
{
    Q_D(QnProxyConnectionProcessor);

    char buffer[1024*64];

    d->socket->setReadTimeOut(IO_TIMEOUT);
    d->socket->setWriteTimeOut(IO_TIMEOUT);
    int readed = d->socket->recv(buffer, sizeof(buffer));
    if (readed < 1)
        return; // no input data

    int lineEndPos = QByteArray::fromRawData(buffer, readed).indexOf('\n');
    if (lineEndPos < 1)
        return; // no input text data

    QList<QByteArray> headers = QByteArray::fromRawData(buffer, lineEndPos).split(' ');
    if (headers.size() != 3)
        return; // unknown protocol

    QUrl url(headers[1]);
    int port = url.port(getDefaultPortByProtocol(url.scheme()));
    if (port == -1 || url.host().isEmpty())
        return; // unknown destination url


    if (!d->dstSocket->connect(url.host().toLatin1().data(), port))
        return; // now answer from destination address

    d->dstSocket->send(buffer, readed);

    fd_set read_set;
    timeval timeVal;
    timeVal.tv_sec  = IO_TIMEOUT/1000;
    timeVal.tv_usec = (IO_TIMEOUT%1000)*1000;
    int nfds = qMax(d->socket->handle(), d->dstSocket->handle()) + 1;

    while (!m_needStop)
    {
        FD_ZERO(&read_set);
        FD_SET(d->socket->handle(), &read_set);
        FD_SET(d->dstSocket->handle(), &read_set);

        int rez = ::select(nfds, &read_set, NULL, NULL, &timeVal);
        if (rez < 1)
            break; // error or timeout
        if (!doProxyData(&read_set, d->socket, d->dstSocket, buffer, sizeof(buffer)))
            break;
        if (!doProxyData(&read_set, d->dstSocket, d->socket, buffer, sizeof(buffer)))
            break;
    }
}
