#include "rtsp_listener.h"
#include "rtsp_connection.h"

QnRtspListener::QnRtspListener(const QHostAddress& address, int port):
    QnTcpListener(address, port)
{

}

QnRtspListener::~QnRtspListener()
{

}

CLLongRunnable* QnRtspListener::createRequestProcessor(TCPSocket* clientSocket, QnTcpListener* owner)
{
    return new QnRtspConnectionProcessor(clientSocket, owner);
}
