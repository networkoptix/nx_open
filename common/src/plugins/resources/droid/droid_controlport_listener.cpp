#include "droid_controlport_listener.h"
#include "utils/network/tcp_connection_priv.h"
#include "droid_stream_reader.h"

QnDroidControlPortListener::QnDroidControlPortListener(const QHostAddress& address, int port):
    QnTcpListener(address, port)
{

}

QnDroidControlPortListener::~QnDroidControlPortListener()
{
    stop();
}

QnTCPConnectionProcessor* QnDroidControlPortListener::createRequestProcessor(AbstractStreamSocket* clientSocket, QnTcpListener* owner)
{
    return new QnDroidControlPortProcessor(clientSocket, owner);
}


// ----------------- QnDroidControlPortProcessor --------------------

class QnDroidControlPortProcessorPrivate: public QnTCPConnectionProcessorPrivate
{
};

QnDroidControlPortProcessor::QnDroidControlPortProcessor(AbstractStreamSocket* socket, QnTcpListener* owner):
    QnTCPConnectionProcessor(socket)
{

}

QnDroidControlPortProcessor::~QnDroidControlPortProcessor()
{
    stop();
}

void QnDroidControlPortProcessor::run()
{
    Q_D(QnDroidControlPortProcessor);
    saveSysThreadID();
    while (!needToStop())
    {
        quint8 recvBuffer[1024*4];
        int readed = d->socket->recv(recvBuffer, sizeof(recvBuffer));
        if (readed > 0)
        {
            quint32 removeIP = d->socket->getPeerAddress().address.ipv4();
            PlDroidStreamReader::setSDPInfo(removeIP, QByteArray((const char*)recvBuffer, readed));
            break;
            
        }
    }
}
