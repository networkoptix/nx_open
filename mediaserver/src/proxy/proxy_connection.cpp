#include <QDebug>
#include <QUrl>
#include <QUrlQuery>

#include "utils/network/tcp_listener.h"
#include "proxy_connection.h"
#include "utils/network/socket.h"
#include "proxy_connection_processor_p.h"
#include "network/universal_tcp_listener.h"
#include "api/app_server_connection.h"
#include <utils/common/systemerror.h>
#include "media_server/server_message_processor.h"

class QnTcpListener;
static const int IO_TIMEOUT = 1000 * 1000;
static const int CONNECT_TIMEOUT = 1000 * 2;

// ----------------------------- QnProxyConnectionProcessor ----------------------------

QnProxyConnectionProcessor::QnProxyConnectionProcessor(QSharedPointer<AbstractStreamSocket> socket, QnTcpListener* owner):
    QnTCPConnectionProcessor(new QnProxyConnectionProcessorPrivate, socket)
{
    Q_D(QnProxyConnectionProcessor);
    d->owner = owner;
}

QnProxyConnectionProcessor::QnProxyConnectionProcessor(QnProxyConnectionProcessorPrivate* priv, QSharedPointer<AbstractStreamSocket> socket, QnTcpListener* owner):
    QnTCPConnectionProcessor(priv, socket)
{
    Q_D(QnProxyConnectionProcessor);
    d->owner = owner;
}


QnProxyConnectionProcessor::~QnProxyConnectionProcessor()
{
	pleaseStop();
    stop();
}

bool QnProxyConnectionProcessor::isProtocol(const QString& protocol) const
{
    return protocol == "http" || protocol == "https" || protocol == "rtsp";
}

int QnProxyConnectionProcessor::getDefaultPortByProtocol(const QString& protocol)
{
    QString p = protocol.toLower();
    if (p == "http")
        return 80;
    else if (p == "https")
        return 443;
    else if (p == "rtsp")
        return 554;
    else 
        return -1;
}

bool QnProxyConnectionProcessor::doProxyData(fd_set* read_set, AbstractStreamSocket* srcSocket, AbstractStreamSocket* dstSocket, char* buffer, int bufferSize)
{
    if(FD_ISSET(srcSocket->handle(), read_set))
    {
        int readed = srcSocket->recv(buffer, bufferSize);
#       ifndef Q_OS_WIN32
            if( readed == -1 && errno == EINTR )
                return true;
#       endif

        if (readed < 1)
            return false;
        dstSocket->send(buffer, readed);
    }
    return true;
}

static bool isLocalAddress(const QString& addr)
{
    return addr == lit("localhost") || addr == lit("127.0.0.1");
}


QString QnProxyConnectionProcessor::connectToRemoteHost(const QString& guid, const QUrl& url, int port )
{
    Q_D(QnProxyConnectionProcessor);
    qWarning() << url.host().toLatin1() << " " << port;
    d->dstSocket = (dynamic_cast<QnUniversalTcpListener*> (d->owner))->getProxySocket(guid, CONNECT_TIMEOUT);
    if (!d->dstSocket) {

        QnServerMessageProcessor* processor = dynamic_cast<QnServerMessageProcessor*> (QnServerMessageProcessor::instance());
#ifdef PROXY_STRICT_IP
        if (!processor->isKnownAddr(url.host()) && ! isLocalAddress(url.host()))
            return QString();
#endif

        d->dstSocket = QSharedPointer<AbstractStreamSocket>(SocketFactory::createStreamSocket(url.scheme() == lit("https")));
        d->dstSocket->setRecvTimeout(CONNECT_TIMEOUT);
        d->dstSocket->setSendTimeout(CONNECT_TIMEOUT);
        if (!d->dstSocket->connect(url.host().toLatin1().data(), port)) {
            d->socket->close();
            return QString(); // now answer from destination address
        }
        return url.host();
    }
    else {
        d->dstSocket->setRecvTimeout(CONNECT_TIMEOUT);
        d->dstSocket->setSendTimeout(CONNECT_TIMEOUT);
        return guid;
    }

    d->dstSocket->setRecvTimeout(IO_TIMEOUT);
    d->dstSocket->setSendTimeout(IO_TIMEOUT);

    return QString();
}

QString QnProxyConnectionProcessor::getServerGuid()
{
    Q_D(QnProxyConnectionProcessor);
#ifdef USE_NX_HTTP
    QString xServerGUID = nx_http::getHeaderValue( d->request.headers, "x-server-guid" );
    QUrlQuery query(d->request.requestLine.url);
    if (xServerGUID.isEmpty() && query.hasQueryItem(lit("serverGuid")))
        xServerGUID = query.queryItemValue(lit("serverGuid"));
#else
    const QString& xServerGUID = d->requestHeaders.value("x-server-guid");
#endif
    return xServerGUID;
}

QUrl QnProxyConnectionProcessor::getDefaultProxyUrl()
{
    Q_D(QnProxyConnectionProcessor);
    return QUrl(lit("http://localhost:%1").arg(d->owner->getPort()));
}

bool QnProxyConnectionProcessor::openProxyDstConnection()
{
    Q_D(QnProxyConnectionProcessor);

    d->dstSocket.clear();

    char* extBuffer = d->clientRequest.data();
    int readed = d->clientRequest.size();

    QUrl url = d->request.requestLine.url;

    if (url.path() == lit("/proxy_api/ec_port"))
    {
        QString data(lit("HTTP 200 OK\r\ncontentLength\r\n port=%1"));
        d->responseBody = QByteArray::number(QnAppServerConnectionFactory::defaultUrl().port());
        sendResponse("HTTP", 200, "text/plain");
        return false;
    }


    QByteArray proxyPattern("proxy");
    if (url.path().startsWith('/'))
        proxyPattern = QByteArray("/proxy");
    QString dstProtocol("http");
    bool addProtocolToUrl = false;
    QString xServerGUID;
    if (url.path().startsWith(proxyPattern))
    {
        QList<QString> proxyParams = url.path().split('/');
        if (proxyParams[0].isEmpty())
            proxyParams.removeAt(0);
        if (proxyParams.size() > 2 && isProtocol(proxyParams[1])) {
            dstProtocol = proxyParams[1];
            proxyParams.removeAt(1);
            addProtocolToUrl = true;
        }
        if (proxyParams.size() >= 2)
        {
            bool isRelativePath = url.host().isEmpty();

            if (proxyParams[1].startsWith("{"))
            {
                xServerGUID = proxyParams[1];
            }
            else {
            	QList<QString> hostAndPort = proxyParams[1].toLower().split(":");
                url.setHost(hostAndPort[0]);
                if (hostAndPort.size() > 1)
                    url.setPort(hostAndPort[1].toInt());
                else
                    url.setPort(getDefaultPortByProtocol(d->protocol));
            }
            if (url.scheme().isEmpty())
                url.setScheme(dstProtocol);
            int proxyEndPos = url.path().indexOf('/', proxyPattern.length()+1);
            url.setPath(url.path().mid(proxyEndPos));
            QByteArray newUrl = url.toString().toUtf8();
            if (isRelativePath) {
                int idx = newUrl.indexOf("://");
                if (idx >= 0)
                    newUrl = newUrl.mid(idx+3);
                idx = newUrl.indexOf("://");
                if (idx >= 0)
                    newUrl = newUrl.mid(idx+3);
                newUrl = newUrl.mid(newUrl.indexOf('/'));
                if (addProtocolToUrl)
                    newUrl = dstProtocol.toLatin1() + QByteArray(":/") + newUrl;
            }

            const char* bufEnd = extBuffer + readed;
            //QByteArray allData = QByteArray::fromRawData(buffer, readed);
            int urlStartPos = d->clientRequest.indexOf(' ')+1;
            int urlEndPos = d->clientRequest.indexOf(' ', urlStartPos+1);
            //int oldUrlLen = urlEndPos - urlStartPos;
            int newEndPos = urlStartPos + newUrl.length();
            memmove(extBuffer + newEndPos, extBuffer + urlEndPos, bufEnd - (extBuffer + urlEndPos));
            readed -= urlEndPos - newEndPos;
            memcpy(extBuffer + urlStartPos, newUrl.data(), newUrl.length());
        }
    }

    int port = url.port(getDefaultPortByProtocol(d->protocol));
    if (port == -1 || url.host().isEmpty()) 
    {
        url = getDefaultProxyUrl();
        if (url.isEmpty()) {
            d->socket->close();
            return false; // unknown destination url
        }
        else
            port = url.port(getDefaultPortByProtocol(d->protocol));
    }

    if (xServerGUID.isEmpty())
        xServerGUID = getServerGuid();
    d->lastConnectedUrl = connectToRemoteHost(xServerGUID , url, port);
    if (d->lastConnectedUrl.isEmpty())
        return false; // invalid dst address

    d->dstSocket->send(d->clientRequest.data(), readed);

    return true;
}

void QnProxyConnectionProcessor::pleaseStop()
{
	Q_D(QnProxyConnectionProcessor);

	QnTCPConnectionProcessor::pleaseStop();
	if (d->socket)
		d->socket->close();
	if (d->dstSocket)
		d->dstSocket->close();
}

void QnProxyConnectionProcessor::run()
{
    Q_D(QnProxyConnectionProcessor);

    d->socket->setRecvTimeout(IO_TIMEOUT);
    d->socket->setSendTimeout(IO_TIMEOUT);

    if (d->clientRequest.isEmpty()) {
        d->socket->close();
        return; // no input data
    }

    parseRequest();

    if (!openProxyDstConnection())
        return;

    bool isWebSocket = nx_http::getHeaderValue( d->request.headers, "Upgrade").toLower() == lit("websocket");
    if (!isWebSocket && (d->protocol.toLower() == "http" || d->protocol.toLower() == "https"))
    {
        doSmartProxy();
    }
    else {
        doRawProxy();
    }

    d->dstSocket->close();
    d->socket->close();
}

void QnProxyConnectionProcessor::doRawProxy()
{
    Q_D(QnProxyConnectionProcessor);

    fd_set read_set;
    char buffer[1024*64];
    int nfds = qMax(d->socket->handle(), d->dstSocket->handle()) + 1;

    while (!m_needStop)
    {
        timeval timeVal;
        timeVal.tv_sec  = IO_TIMEOUT/1000;
        timeVal.tv_usec = (IO_TIMEOUT%1000)*1000;

        FD_ZERO(&read_set);
        FD_SET(d->socket->handle(), &read_set);
        FD_SET(d->dstSocket->handle(), &read_set);

        int rez = ::select(nfds, &read_set, NULL, NULL, &timeVal);
#ifndef Q_OS_WIN32
        if( rez == -1 && errno == EINTR )
            continue;
#endif

        if (rez < 1)
            break; // error or timeout
        if (!doProxyData(&read_set, d->socket.data(), d->dstSocket.data(), buffer, sizeof(buffer)))
            break;
        if (!doProxyData(&read_set, d->dstSocket.data(), d->socket.data(), buffer, sizeof(buffer)))
            break;

    }

}

bool QnProxyConnectionProcessor::isSameDstAddr()
{
    Q_D(QnProxyConnectionProcessor);

    QString xServerGUID = getServerGuid();
    QUrl url = d->request.requestLine.url;

    QByteArray proxyPattern("proxy");
    if (url.path().startsWith('/'))
        proxyPattern = QByteArray("/proxy");
    QString dstProtocol("http");
    if (url.path().startsWith(proxyPattern))
    {
        QList<QString> proxyParams = url.path().split('/');
        if (proxyParams[0].isEmpty())
            proxyParams.removeAt(0);
        if (proxyParams.size() > 2 && isProtocol(proxyParams[1])) {
            dstProtocol = proxyParams[1];
            proxyParams.removeAt(1);
        }
        if (proxyParams.size() >= 2)
        {
            bool isRelativePath = url.host().isEmpty();
            if (proxyParams[1].startsWith("{")) {
                xServerGUID = proxyParams[1];
            }
            else {
                QList<QString> hostAndPort = proxyParams[1].toLower().split(":");
                url.setHost(hostAndPort[0]);
                if (hostAndPort.size() > 1)
                    url.setPort(hostAndPort[1].toInt());
                else
                    url.setPort(getDefaultPortByProtocol(d->protocol));
            }
            if (url.scheme().isEmpty())
                url.setScheme(dstProtocol);
            int proxyEndPos = url.path().indexOf('/', proxyPattern.length()+1);
            url.setPath(url.path().mid(proxyEndPos));
        }
    }

    int port = url.port(getDefaultPortByProtocol(d->protocol));
    if (port == -1 || url.host().isEmpty()) 
        url = getDefaultProxyUrl();

    return xServerGUID == d->lastConnectedUrl || url.host() == d->lastConnectedUrl;
}

void QnProxyConnectionProcessor::doSmartProxy()
{
    Q_D(QnProxyConnectionProcessor);

    fd_set read_set;
    char buffer[1024*64];
    int nfds = qMax(d->socket->handle(), d->dstSocket->handle()) + 1;

    d->clientRequest.clear();

    while (!m_needStop)
    {
        timeval timeVal;
        timeVal.tv_sec  = IO_TIMEOUT/1000;
        timeVal.tv_usec = (IO_TIMEOUT%1000)*1000;

        FD_ZERO(&read_set);
        FD_SET(d->socket->handle(), &read_set);
        FD_SET(d->dstSocket->handle(), &read_set);

        int rez = ::select(nfds, &read_set, NULL, NULL, &timeVal);
#ifndef Q_OS_WIN32
        if( rez == -1 && errno == EINTR )
            continue;
#endif
        if (rez < 1)
            break; // error or timeout

        if(FD_ISSET(d->socket->handle(), &read_set))
        {
            int readed = d->socket->recv(d->tcpReadBuffer, TCP_READ_BUFFER_SIZE);
#           ifndef Q_OS_WIN32
                if( readed == -1 && errno == EINTR )
                    continue;
#           endif

            if (readed < 1) 
                break;
            d->clientRequest.append((const char*) d->tcpReadBuffer, readed);
            if (isFullMessage(d->clientRequest)) 
            {
                parseRequest();
                bool isWebSocket = nx_http::getHeaderValue( d->request.headers, "Upgrade").toLower() == lit("websocket");
                if (isSameDstAddr()) 
                {
                    // remove proxy prefix from the request if exists
                    int urlPos = d->clientRequest.indexOf(" ");
                    if (urlPos > 0) {
                        urlPos++;
                        QByteArray tmp = QByteArray::fromRawData(d->clientRequest.data() + urlPos, d->clientRequest.size() - urlPos);
                        if (tmp.startsWith("/proxy") || tmp.startsWith("proxy")) 
                        {
                            int prefixEnd = tmp.indexOf("/", 1);
                            if (prefixEnd > 0) {
                                int prefixEnd2 = tmp.indexOf("/", prefixEnd+1);
                                if (prefixEnd2 > 0) {
                                    QByteArray protocol = tmp.mid(prefixEnd+1, prefixEnd2 - prefixEnd-1);
                                    if (isProtocol(protocol)) {
                                        prefixEnd2 = tmp.indexOf("/", prefixEnd2+1);
                                        d->clientRequest = d->clientRequest.remove(urlPos, prefixEnd2);
                                    }
                                    else {
                                        d->clientRequest = d->clientRequest.remove(urlPos, prefixEnd);
                                    }
                                }
                            }
                        }
                    }

                    d->dstSocket->send(d->clientRequest);
                    if (isWebSocket) 
                    {
                        if (!doProxyData(&read_set, d->dstSocket.data(), d->socket.data(), buffer, sizeof(buffer)))
                            break; // send rest of data
                        doRawProxy(); // switch to binary mode
                        return;
                    }
                }
                else {
                    // new server
                    if(!openProxyDstConnection())
                        break;
                    if (isWebSocket) 
                    {
                        doRawProxy(); // switch to binary mode
                        return;
                    }
                }
                d->clientRequest.clear();
            }
        }

        if (!doProxyData(&read_set, d->dstSocket.data(), d->socket.data(), buffer, sizeof(buffer)))
            break;

    }
}
