#include "progressive_downloading_server.h"
#include "utils/network/tcp_connection_priv.h"
#include "utils/network/tcp_listener.h"

static const int CONNECTION_TIMEOUT = 1000 * 5;

class QnProgressiveDownloadingProcessor::QnProgressiveDownloadingProcessorPrivate: public QnTCPConnectionProcessor::QnTCPConnectionProcessorPrivate
{
};

QnProgressiveDownloadingProcessor::QnProgressiveDownloadingProcessor(TCPSocket* socket, QnTcpListener* _owner):
QnTCPConnectionProcessor(new QnProgressiveDownloadingProcessorPrivate, socket, _owner)
{
    Q_D(QnProgressiveDownloadingProcessor);
    d->socketTimeout = CONNECTION_TIMEOUT;
}

QnProgressiveDownloadingProcessor::~QnProgressiveDownloadingProcessor()
{

}

void QnProgressiveDownloadingProcessor::run()
{
    Q_D(QnProgressiveDownloadingProcessor);
    if (readRequest())
    {
        parseRequest();
        d->responseBody.clear();
        int rez = CODE_OK;
        sendResponse("HTTP", rez, "application/xml");
    }

    d->socket->close();
    m_runing = false;
}
