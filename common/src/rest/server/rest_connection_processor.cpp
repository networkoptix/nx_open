#include <QUrl>

#include "rest_connection_processor.h"
#include "utils/network/tcp_connection_priv.h"
#include "utils/network/tcp_listener.h"
#include "rest_server.h"
#include "request_handler.h"
#include <QTime>

static const int CONNECTION_TIMEOUT = 60 * 1000;

QnRestConnectionProcessor::Handlers QnRestConnectionProcessor::m_handlers;

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

    bool ready = true;
    if (d->clientRequest.isEmpty())
        ready = readRequest();

    while (1)
    {
        bool isKeepAlive = false;
        if (ready)
        {
            parseRequest();
            isKeepAlive = d->requestHeaders.value(QLatin1String("Connection")).toLower() == QString(QLatin1String("keep-alive"));
            if (isKeepAlive) {
                d->responseHeaders.addValue(QLatin1String("Connection"), QLatin1String("Keep-Alive"));
                d->responseHeaders.addValue(QLatin1String("Keep-Alive"), QString(QLatin1String("timeout=%1")).arg(d->socketTimeout/1000));
            }

            d->responseBody.clear();
            int rez = CODE_OK;
            QByteArray contentType = "application/xml";
            QByteArray contentEncoding;
            QUrl url = getDecodedUrl();
            QnRestRequestHandlerPtr handler = findHandler(url.path());
            if (handler) 
            {
                QList<QPair<QString, QString> > params = url.queryItems();
                if (d->owner->authenticate(d->requestHeaders, d->responseHeaders))
                {
                    if (d->requestHeaders.method().toUpper() == QLatin1String("GET")) {
                        rez = handler->executeGet(url.path(), params, d->responseBody, contentType, contentEncoding);
                    }
                    else if (d->requestHeaders.method().toUpper() == QLatin1String("POST")) {
                        rez = handler->executePost(url.path(), params, d->requestBody, d->responseBody, contentType, contentEncoding);
                    }
                    else {
                        qWarning() << "Unknown REST method " << d->requestHeaders.method();
                        contentType = "text/plain";
                        d->responseBody = "Invalid HTTP method";
                        rez = CODE_NOT_FOUND;
                    }
                }
                else {
                    contentType = "text/html";
                    d->responseBody = STATIC_UNAUTHORIZED_HTML;
                    rez = CODE_AUTH_REQUIRED;
                }
            }
            else {
                if (url.path() != QLatin1String("/api/ping/"))
                    qWarning() << "Unknown REST path " << url.path();
                contentType = "text/html";
                d->responseBody.clear();
                d->responseBody.append("<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n");
                d->responseBody.append("<html lang=\"en\" xmlns=\"http://www.w3.org/1999/xhtml\">\n");
                d->responseBody.append("<head>\n");
                d->responseBody.append("<b>Requested method is absent. Allowed methods:</b>\n");
                d->responseBody.append("</head>\n");
                d->responseBody.append("<body>\n");

                d->responseBody.append("<TABLE BORDER=\"1\" CELLSPACING=\"0\">\n");
                for(Handlers::const_iterator itr = m_handlers.begin(); itr != m_handlers.end(); ++itr)
                {
                    QString str = itr.key();
                    if (str.startsWith(QLatin1String("api/")))
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
            sendResponse("HTTP", rez, contentType, contentEncoding, false);
        }
        if (!isKeepAlive)
            break;
        ready = readRequest();
    }
    d->socket->close();
}

void QnRestConnectionProcessor::registerHandler(const QString& path, QnRestRequestHandler* handler)
{
    m_handlers.insert(path, QnRestRequestHandlerPtr(handler));
    handler->setPath(path);
}

QnRestRequestHandlerPtr QnRestConnectionProcessor::findHandler(QString path)
{
    if (path.startsWith(L'/'))
        path = path.mid(1);
    if (path.endsWith(L'/'))
        path = path.left(path.length()-1);

    for (Handlers::iterator i = m_handlers.begin();i != m_handlers.end(); ++i)
    {
        QRegExp expr(i.key(), Qt::CaseSensitive, QRegExp::Wildcard);
        if (expr.indexIn(path) != -1)
            return i.value();
    }

    return QnRestRequestHandlerPtr();
}
