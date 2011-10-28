#include <QUrl>

#include "rest_connection_processor.h"
#include "utils/network/tcp_connection_priv.h"
#include "utils/network/tcp_listener.h"
#include "rest_server.h"

struct QnRestConnectionProcessor::QnRestConnectionProcessorPrivate: public QnTCPConnectionProcessor::QnTCPConnectionProcessorPrivate
{
};

QnRestConnectionProcessor::QnRestConnectionProcessor(TCPSocket* socket, QnTcpListener* _owner):
    QnTCPConnectionProcessor(new QnRestConnectionProcessorPrivate, socket, _owner)
{
    Q_D(QnRestConnectionProcessor);
}

QnRestConnectionProcessor::~QnRestConnectionProcessor()
{

}

void QnRestConnectionProcessor::run()
{
    Q_D(QnRestConnectionProcessor);
    bool ready = false;
    while (!m_needStop && d->socket->isConnected())
    {
        int readed = d->socket->recv(d->tcpReadBuffer, TCP_READ_BUFFER_SIZE);
        if (readed > 0) {
            d->clientRequest.append((const char*) d->tcpReadBuffer, readed);
            if (isFullMessage())
            {
                ready = true;
                break;
            }
        }
    }
    if (ready)
    {
        parseRequest();
        QUrl url(d->requestHeaders.path());

        QnRestRequestHandler* handler = static_cast<QnRestServer*>(d->owner)->findHandler(url.path());
        int rez = CODE_OK;
        d->responseBody.clear();
        if (handler) {
            if (d->requestHeaders.method().toUpper() == "GET")
                rez = handler->executeGet(url.queryItems(), d->responseBody);
            else if (d->requestHeaders.method().toUpper() == "POST")
                rez = handler->executePost(url.queryItems(), d->requestBody, d->responseBody);
            else {
                qWarning() << "Unknown REST method " << d->requestHeaders.method();
                rez = CODE_NOT_FOUND;
            }
        }
        else {
            qWarning() << "Unknown REST path " << url.path();
            rez = CODE_NOT_FOUND;
        }
        sendResponse("HTTP", rez, "application/xml");
    }

    m_runing = false;
}
