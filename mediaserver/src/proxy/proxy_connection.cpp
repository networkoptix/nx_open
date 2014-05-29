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

bool QnProxyConnectionProcessor::doProxyData(AbstractStreamSocket* srcSocket, AbstractStreamSocket* dstSocket, char* buffer, int bufferSize)
{
    int readed = srcSocket->recv(buffer, bufferSize);
#ifndef Q_OS_WIN32
        if( readed == -1 && errno == EINTR )
            return true;
#endif

    if (readed < 1)
        return false;
    for( ;; )
    {
        int sended = dstSocket->send(buffer, readed);
        if( sended < 0 )
        {
            if( SystemError::getLastOSErrorCode() == SystemError::interrupted )
                continue;   //retrying interrupted call
            NX_LOG( lit("QnProxyConnectionProcessor::doProxyData. Socket error: %1").arg(SystemError::getLastOSErrorText()), cl_logDEBUG1 );
            return false;
        }
        if( sended == 0 )
            return false;   //socket closed
        if( sended == readed )
            return true;    //sent everything
        buffer += sended;
        readed -= sended;
    }
}

#ifdef PROXY_STRICT_IP
static bool isLocalAddress(const QString& addr)
{
    return addr == lit("localhost") || addr == lit("127.0.0.1");
}
#endif

QString QnProxyConnectionProcessor::connectToRemoteHost(const QString& guid, const QUrl& url)
{
    Q_D(QnProxyConnectionProcessor);
    qWarning() << url.host().toLatin1() << " " << url.port();
    d->dstSocket = (dynamic_cast<QnUniversalTcpListener*> (d->owner))->getProxySocket(guid, CONNECT_TIMEOUT);
    if (!d->dstSocket) {

#ifdef PROXY_STRICT_IP
        QnServerMessageProcessor* processor = dynamic_cast<QnServerMessageProcessor*> (QnServerMessageProcessor::instance());
        if (processor && !processor->isKnownAddr(url.host()) && ! isLocalAddress(url.host()))
            return QString();
#endif

        d->dstSocket = QSharedPointer<AbstractStreamSocket>(SocketFactory::createStreamSocket(url.scheme() == lit("https")));
        d->dstSocket->setRecvTimeout(CONNECT_TIMEOUT);
        d->dstSocket->setSendTimeout(CONNECT_TIMEOUT);
        if (!d->dstSocket->connect(url.host().toLatin1().data(), url.port())) {
            d->socket->close();
            return QString(); // now answer from destination address
        }
        return url.toString();
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


QUrl QnProxyConnectionProcessor::getDefaultProxyUrl()
{
    Q_D(QnProxyConnectionProcessor);
    return QUrl(lit("http://localhost:%1").arg(d->owner->getPort()));
}

bool QnProxyConnectionProcessor::updateClientRequest(QUrl& dstUrl, QString& xServerGUID)
{
    Q_D(QnProxyConnectionProcessor);

    QUrl url = d->request.requestLine.url;
    QString host = url.host();
    QString urlPath = url.path();

    if (urlPath.startsWith("proxy") || urlPath.startsWith("/proxy"))
    {
        int proxyEndPos = urlPath.indexOf('/', 1); // remove proxy prefix
        int protocolEndPos = urlPath.indexOf('/', proxyEndPos+1); // remove proxy prefix
        if (protocolEndPos == -1)
            return false;
        int hostEndPos = urlPath.indexOf('/', protocolEndPos+1); // remove proxy prefix
        if (hostEndPos == -1)
            hostEndPos = urlPath.size();

        QString protocol = urlPath.mid(proxyEndPos+1, protocolEndPos - proxyEndPos-1);
        host = urlPath.mid(protocolEndPos+1, hostEndPos - protocolEndPos-1);
        if (host.startsWith("{"))
            xServerGUID = host;

        urlPath = urlPath.mid(hostEndPos);

        // get dst ip and port
        QStringList hostAndPort = host.split(':');
        int port = hostAndPort.size() > 1 ? hostAndPort[1].toInt() : getDefaultPortByProtocol(protocol);

        dstUrl = QUrl(lit("%1://%2:%3").arg(protocol).arg(hostAndPort[0]).arg(port));
    }
    else {
        int defaultPort = getDefaultPortByProtocol(url.scheme());
        dstUrl = QUrl(lit("%1://%2:%3").arg(url.scheme()).arg(url.host()).arg(url.port(defaultPort)));
    }

    if (urlPath.isEmpty())
        urlPath = "/";
    QString query = url.query();
    if (!query.isEmpty()) {
        urlPath += lit("?");
        urlPath += query;
    }
    d->request.requestLine.url = urlPath;

    for (nx_http::HttpHeaders::iterator itr = d->request.headers.begin(); itr != d->request.headers.end(); ++itr)
    {
        if (itr->first.toLower() == "host" && !host.isEmpty())
            itr->second = host.toUtf8();
        else if (itr->first == "x-server-guid")
            xServerGUID = itr->second;
    }

    d->clientRequest.clear();
    d->request.serialize(&d->clientRequest);

    return true;
}

bool QnProxyConnectionProcessor::openProxyDstConnection()
{
    Q_D(QnProxyConnectionProcessor);

    d->dstSocket.clear();

    // update source request
    QUrl dstUrl;
    QString xServerGUID;
    if (!updateClientRequest(dstUrl, xServerGUID))
    {
        d->socket->close();
        return false;
    }

    d->lastConnectedUrl = connectToRemoteHost(xServerGUID , dstUrl);
    if (d->lastConnectedUrl.isEmpty())
        return false; // invalid dst address

    d->dstSocket->send(d->clientRequest.data(), d->clientRequest.size());

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

    d->pollSet.add( d->socket.data(), aio::etRead );
    d->pollSet.add( d->dstSocket.data(), aio::etRead );

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

    char buffer[1024*64];

    while (!m_needStop)
    {
        int rez = d->pollSet.poll( IO_TIMEOUT );
        if( rez == -1 && SystemError::getLastOSErrorCode() == SystemError::interrupted )
            continue;

        if (rez < 1)
            return; // error or timeout
        for( aio::PollSet::const_iterator
            it = d->pollSet.begin();
            it != d->pollSet.end();
            ++it )
        {
            if( it.eventType() != aio::etRead )
                return;
            if( it.socket() == d->socket )
                if (!doProxyData(d->socket.data(), d->dstSocket.data(), buffer, sizeof(buffer)))
                    return;
            if( it.socket() == d->dstSocket )
                if (!doProxyData(d->dstSocket.data(), d->socket.data(), buffer, sizeof(buffer)))
                    return;
        }
    }
}

void QnProxyConnectionProcessor::doSmartProxy()
{
    Q_D(QnProxyConnectionProcessor);

    char buffer[1024*64];
    d->clientRequest.clear();

    while (!m_needStop)
    {
        int rez = d->pollSet.poll( IO_TIMEOUT );
        if( rez == -1 && SystemError::getLastOSErrorCode() == SystemError::interrupted )
            continue;
        if (rez < 1)
            return; // error or timeout

        for( aio::PollSet::const_iterator
            it = d->pollSet.begin();
            it != d->pollSet.end();
            ++it )
        {
            if( it.eventType() != aio::etRead )
                return;
            if( it.socket() == d->socket )
            {
                int readed = d->socket->recv(d->tcpReadBuffer, TCP_READ_BUFFER_SIZE);
                if (readed < 1) 
                    return;

                d->clientRequest.append((const char*) d->tcpReadBuffer, readed);
                if (isFullMessage(d->clientRequest)) 
                {
                    parseRequest();
                    QString path = d->request.requestLine.url.path();
                    // parse next request and change dst if required
                    QUrl dstUrl;
                    QString xServerGUID;
                    updateClientRequest(dstUrl, xServerGUID);
                    bool isWebSocket = nx_http::getHeaderValue( d->request.headers, "Upgrade").toLower() == lit("websocket");
                    bool isSameAddr = d->lastConnectedUrl == xServerGUID || d->lastConnectedUrl == dstUrl;
                    if (isSameAddr) 
                    {
                        d->dstSocket->send(d->clientRequest);
                        if (isWebSocket) 
                        {
                            if( rez == 2 ) //same as FD_ISSET(d->dstSocket->handle(), &read_set), since we have only 2 sockets
                                if (!doProxyData(d->dstSocket.data(), d->socket.data(), buffer, sizeof(buffer)))
                                    return; // send rest of data
                            doRawProxy(); // switch to binary mode
                            return;
                        }
                    }
                    else {
                        // new server
                        d->lastConnectedUrl = connectToRemoteHost(xServerGUID , dstUrl);
                        if (d->lastConnectedUrl.isEmpty()) {
                            d->socket->close();
                            return; // invalid dst address
                        }

                        d->dstSocket->send(d->clientRequest.data(), d->clientRequest.size());

                        if (isWebSocket) 
                        {
                            doRawProxy(); // switch to binary mode
                            return;
                        }
                    }
                    d->clientRequest.clear();
                }
            }
            else if( it.socket() == d->dstSocket )
            {
                if (!doProxyData(d->dstSocket.data(), d->socket.data(), buffer, sizeof(buffer)))
                    return;
            }
        }
    }
}
