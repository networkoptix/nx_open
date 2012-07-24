#include <QUrl>

#include "rest_connection_processor.h"
#include "utils/network/tcp_connection_priv.h"
#include "utils/network/tcp_listener.h"
#include "rest_server.h"
#include "request_handler.h"
#include <QTime>

static const int CONNECTION_TIMEOUT = 60 * 1000;
static const int MAX_REQUEST_SIZE = 1024*1024*15;

class QnRestConnectionProcessor::QnRestConnectionProcessorPrivate: public QnTCPConnectionProcessor::QnTCPConnectionProcessorPrivate
{
};

QnRestConnectionProcessor::QnRestConnectionProcessor(TCPSocket* socket, QnTcpListener* _owner):
    QnTCPConnectionProcessor(new QnRestConnectionProcessorPrivate, socket, _owner)
{
    Q_D(QnRestConnectionProcessor);
    d->socketTimeout = CONNECTION_TIMEOUT;
}

QnRestConnectionProcessor::~QnRestConnectionProcessor()
{
    stop();
}

void QnRestConnectionProcessor::run()
{
    Q_D(QnRestConnectionProcessor);
    QTime globalTimeout;
    while (1)
    {
        globalTimeout.restart();
        d->requestHeaders = QHttpRequestHeader();
        d->responseHeaders = QHttpResponseHeader();
        d->clientRequest.clear();
        d->requestBody.clear();
        d->responseBody.clear();
        bool ready = false;
        while (!m_needStop && d->socket->isConnected())
        {
            int readed = d->socket->recv(d->tcpReadBuffer, TCP_READ_BUFFER_SIZE);
            if (readed > 0) 
            {
                globalTimeout.restart();
                d->clientRequest.append((const char*) d->tcpReadBuffer, readed);
                if (isFullMessage(d->clientRequest))
                {
                    ready = true;
                    break;
                }
                else if (d->clientRequest.size() > MAX_REQUEST_SIZE)
                {
                    qWarning() << "Too large HTTP client request. Ignoring";
                    break;
                }
            }
            else //if (globalTimeout.elapsed() > CONNECTION_TIMEOUT)
            {
                break;
            }
        }

        bool isKeepAlive = false;
        if (ready)
        {
            parseRequest();
            isKeepAlive = d->requestHeaders.value("Connection").toLower() == QString("keep-alive");
            if (isKeepAlive) {
                d->responseHeaders.addValue(QString("Connection"), QString("Keep-Alive"));
                d->responseHeaders.addValue(QString("Keep-Alive"), QString("timeout=%1").arg(d->socketTimeout/1000));
            }

            QByteArray data = d->requestHeaders.path().toUtf8();
            data = data.replace("+", "%20");
            QUrl url = QUrl::fromEncoded(data);

            d->responseBody.clear();
            int rez = CODE_OK;
            QByteArray encoding = "application/xml";

            QnRestRequestHandler* handler = static_cast<QnRestServer*>(d->owner)->findHandler(url.path());
            if (handler) 
            {
                QList<QPair<QString, QString> > params = url.queryItems();
                if (d->owner->authenticate(d->requestHeaders, d->responseHeaders))
                {
                    if (d->requestHeaders.method().toUpper() == "GET") {
                        rez = handler->executeGet(url.path(), params, d->responseBody);
                    }
                    else if (d->requestHeaders.method().toUpper() == "POST") {
                        rez = handler->executePost(url.path(), params, d->requestBody, d->responseBody);
                    }
                    else {
                        qWarning() << "Unknown REST method " << d->requestHeaders.method();
                        encoding = "plain/text";
                        d->responseBody = "Invalid HTTP method";
                        rez = CODE_NOT_FOUND;
                    }
                }
                else {
                    encoding = "text/html";
                    d->responseBody = STATIC_UNAUTHORIZED_HTML;
                    rez = CODE_AUTH_REQUIRED;
                }
            }
            else {
                qWarning() << "Unknown REST path " << url.path();
                encoding = "text/html";
                d->responseBody.clear();
                d->responseBody.append("<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n");
                d->responseBody.append("<html lang=\"en\" xmlns=\"http://www.w3.org/1999/xhtml\">\n");
                d->responseBody.append("<head>\n");
                d->responseBody.append("<b>Requested method is absent. Allowed methods:</b>\n");
                d->responseBody.append("</head>\n");
                d->responseBody.append("<body>\n");

                d->responseBody.append("<TABLE BORDER=\"1\" CELLSPACING=\"0\">\n");
                const QnRestServer::Handlers& allHandlers = static_cast<QnRestServer*>(d->owner)->allHandlers();
                for(QnRestServer::Handlers::const_iterator itr = allHandlers.begin(); itr != allHandlers.end(); ++itr)
                {
                    QString str = itr.key();
                    if (str.startsWith("api/"))
                    {
                        d->responseBody.append("<TR><TD>");
                        d->responseBody.append(str.toAscii());
                        d->responseBody.append("<TD>");
                        d->responseBody.append(itr.value()->description(d->socket));
                        d->responseBody.append("</TD>");
                        d->responseBody.append("</TD></TR>\n");
                    }
                }
                d->responseBody.append("</TABLE>\n");

                d->responseBody.append("</body>\n");
                d->responseBody.append("</html>\n");
                rez = CODE_NOT_FOUND;
            }
            sendResponse("HTTP", rez, encoding);
        }
        if (!isKeepAlive)
            break;
    }
    d->socket->close();
    m_runing = false;
}

void QnRestConnectionProcessor::parseRequest()
{
    Q_D(QnRestConnectionProcessor);
    QnTCPConnectionProcessor::parseRequest();
}
