#include "droid_controlport_listener.h"
#include "utils/network/tcp_connection_priv.h"
#include "droid_stream_reader.h"

QnDroidControlPortListener::QnDroidControlPortListener(const QHostAddress& address, int port):
    QnTcpListener(address, port)
{

}

QnTCPConnectionProcessor* QnDroidControlPortListener::createRequestProcessor(TCPSocket* clientSocket, QnTcpListener* owner)
{
    return new QnDroidControlPortProcessor(clientSocket, owner);
}


// ----------------- QnDroidControlPortProcessor --------------------

class QnDroidControlPortProcessor::QnDroidControlPortProcessorPrivate: public QnTCPConnectionProcessor::QnTCPConnectionProcessorPrivate
{
};

QnDroidControlPortProcessor::QnDroidControlPortProcessor(TCPSocket* socket, QnTcpListener* owner):
    QnTCPConnectionProcessor(socket, owner)
{

}


void QnDroidControlPortProcessor::run()
{
    Q_D(QnDroidControlPortProcessor);
    while (!m_needStop)
    {
        quint8 recvBuffer[1024*4];
        int readed = d->socket->recv(recvBuffer, sizeof(recvBuffer));
        if (readed > 0)
        {
            quint32 removeIP = d->socket->getPeerAddressUint();
            PlDroidStreamReader::setSDPInfo(removeIP, QByteArray((const char*)recvBuffer, readed));
            break;
            
        }
    }
}
