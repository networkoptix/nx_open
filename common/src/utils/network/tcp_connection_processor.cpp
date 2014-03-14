#include "tcp_connection_processor.h"

#include <QtCore/QTime>

#include <utils/common/log.h>
#include <utils/network/http/httptypes.h>

#include "tcp_listener.h"
#include "tcp_connection_priv.h"
#include "err.h"

#ifndef Q_OS_WIN
#   include <netinet/tcp.h>
#endif

static const int MAX_REQUEST_SIZE = 1024*1024*15;


QnTCPConnectionProcessor::QnTCPConnectionProcessor(QSharedPointer<AbstractStreamSocket> socket):
    d_ptr(new QnTCPConnectionProcessorPrivate)
{
    Q_D(QnTCPConnectionProcessor);
    d->socket = socket;
    d->chunkedMode = false;
}

QnTCPConnectionProcessor::QnTCPConnectionProcessor(QnTCPConnectionProcessorPrivate* dptr, QSharedPointer<AbstractStreamSocket> socket):
    d_ptr(dptr)
{
    Q_D(QnTCPConnectionProcessor);
    d->socket = socket;
    //d->socket->setNoDelay(true);
    d->chunkedMode = false;
}


QnTCPConnectionProcessor::~QnTCPConnectionProcessor()
{
    stop();
    delete d_ptr;
}

int QnTCPConnectionProcessor::isFullMessage(const QByteArray& message)
{
    QByteArray lRequest = message.toLower();
    QByteArray delimiter = "\n";
    int pos = lRequest.indexOf(delimiter);
    if (pos == -1)
        return 0;
    if (pos > 0 && lRequest[pos-1] == '\r')
        delimiter = "\r\n";
    int contentLen = 0;
    int contentLenPos = lRequest.indexOf("content-length");
    if (contentLenPos >= 0)
    {
        int posStart = -1;
        int posEnd = -1;
        for (int i = contentLenPos+1; i < lRequest.length(); ++i)
        {
            if (lRequest[i] >= '0' && lRequest[i] <= '9')
            {
                if (posStart == -1)
                    posStart = i;
                posEnd = i;
            }
            else if (lRequest[i] == '\n' || lRequest[i] == '\r')
                break;
        }
        if (posStart >= 0)
            contentLen = lRequest.mid(posStart, posEnd - posStart+1).toInt();
    }
    QByteArray dblDelim = delimiter + delimiter;
    //return lRequest.endsWith(dblDelim) && (!contentLen || lRequest.indexOf(dblDelim) < lRequest.length()-dblDelim.length());
    if (lRequest.indexOf(dblDelim) == -1)
        return 0;
    int expectedSize = lRequest.indexOf(dblDelim) + dblDelim.size() + contentLen;
    if (expectedSize > lRequest.size())
        return 0;
    else 
        return expectedSize;
}

void QnTCPConnectionProcessor::parseRequest()
{
    Q_D(QnTCPConnectionProcessor);
//    qDebug() << "Client request from " << d->socket->getPeerAddress().address.toString();
//    qDebug() << d->clientRequest;

#ifdef USE_NX_HTTP
    d->request = nx_http::Request();
    if( !d->request.parse( d->clientRequest ) )
    {
        qWarning() << Q_FUNC_INFO << "Invalid request format.";
        return;
    }
    QList<QByteArray> versionParts = d->request.requestLine.version.split('/');
    d->protocol = versionParts[0];
    d->requestBody = d->request.messageBody;
#else
    QList<QByteArray> lines = d->clientRequest.split('\n');
    bool firstLine = true;
    foreach (const QByteArray& l, lines)
    {
        QByteArray line = l.trimmed();
        if (line.isEmpty())
            break;
        if (firstLine)
        {
            QList<QByteArray> params = line.split(' ');
            if (params.size() != 3)
            {
                qWarning() << Q_FUNC_INFO << "Invalid request format.";
                return;
            }
            QList<QByteArray> version = params[2].split('/');
            d->protocol = version[0];
            int major = 1;
            int minor = 0;
            if (version.size() > 1)
            {
                QList<QByteArray> v = version[1].split('.');
                major = v[0].toInt();
                if (v.length() > 1)
                    minor = v[1].toInt();
            }
            d->requestHeaders = QHttpRequestHeader(QLatin1String(params[0]), QLatin1String(params[1]), major, minor);
            firstLine = false;
        }
        else
        {
            QByteArray headerName;
            QByteArray headerValue;
            nx_http::parseHeader( &headerName, &headerValue, line );
            d->requestHeaders.addValue( QLatin1String(headerName), QLatin1String(headerValue) );
        }
    }
    QByteArray delimiter = "\n";
    int pos = d->clientRequest.indexOf(delimiter);
    if (pos == -1)
        return;
    if (pos > 0 && d->clientRequest[pos-1] == '\r')
        delimiter = "\r\n";
    QByteArray dblDelim = delimiter + delimiter;
    int bodyStart = d->clientRequest.indexOf(dblDelim);
    if (bodyStart >= 0 && d->requestHeaders.value(QLatin1String("content-length")).toInt() > 0)
        d->requestBody = d->clientRequest.mid(bodyStart + dblDelim.length());
#endif
}

/*
void QnTCPConnectionProcessor::bufferData(const char* data, int size)
{
    Q_D(QnTCPConnectionProcessor);
    d->sendBuffer.write(data, size);
}

QnByteArray& QnTCPConnectionProcessor::getSendBuffer()
{
    Q_D(QnTCPConnectionProcessor);
    return d->sendBuffer;
}

void QnTCPConnectionProcessor::sendBuffer()
{
    Q_D(QnTCPConnectionProcessor);
    sendData(d->sendBuffer.data(), d->sendBuffer.size());
}

void QnTCPConnectionProcessor::clearBuffer()
{
    Q_D(QnTCPConnectionProcessor);
    d->sendBuffer.clear();
}
*/
bool QnTCPConnectionProcessor::sendBuffer(const QnByteArray& sendBuffer)
{
    Q_D(QnTCPConnectionProcessor);

    QMutexLocker lock(&d->sockMutex);
    return sendData(sendBuffer.data(), sendBuffer.size());
}

bool QnTCPConnectionProcessor::sendBuffer(const QByteArray& sendBuffer)
{
    Q_D(QnTCPConnectionProcessor);

    QMutexLocker lock(&d->sockMutex);
    int bytesSent = 0;
    return sendData(sendBuffer.data(), sendBuffer.size());
}

bool QnTCPConnectionProcessor::sendData(const char* data, int size)
{
    Q_D(QnTCPConnectionProcessor);
    while (!needToStop() && size > 0 && d->socket->isConnected())
    {
        int sended = 0;
        sended = d->socket->send(data, size);
        if( sended <= 0 )
            break;

#ifdef DEBUG_RTSP
        dumpRtspData(data, sended);
#endif
        data += sended;
        size -= sended;
    }
    return d->socket->isConnected();
}


QString QnTCPConnectionProcessor::extractPath() const
{
    Q_D(const QnTCPConnectionProcessor);
#ifdef USE_NX_HTTP
    return d->request.requestLine.url.path();
#else
    return extractPath(d->requestHeaders.path());
#endif
}

QString QnTCPConnectionProcessor::extractPath(const QString& fullUrl)
{
    int pos = fullUrl.indexOf(QLatin1String("://"));
    if (pos == -1)
        return fullUrl;
    pos = fullUrl.indexOf(QLatin1Char('/'), pos+3);
    if (pos == -1)
        return QString();
    return fullUrl.mid(pos+1);
}

void QnTCPConnectionProcessor::sendResponse(const QByteArray& transport, int code, const QByteArray& contentType, const QByteArray& contentEncoding, bool displayDebug)
{
    Q_D(QnTCPConnectionProcessor);

#ifdef USE_NX_HTTP
    d->response.statusLine.version = d->request.requestLine.version;
    d->response.statusLine.statusCode = code;
    d->response.statusLine.reasonPhrase = nx_http::StatusCode::toString((nx_http::StatusCode::Value)code);
    if (d->chunkedMode)
        d->response.headers.insert( nx_http::HttpHeader( "Transfer-Encoding", "chunked" ) );

    if (!contentEncoding.isEmpty())
        d->response.headers.insert( nx_http::HttpHeader( "Content-Encoding", contentEncoding ) );
    if (!contentType.isEmpty())
        d->response.headers.insert( nx_http::HttpHeader( "Content-Type", contentType ) );
    if (!d->chunkedMode)
        d->response.headers.insert( nx_http::HttpHeader( "Content-Length", QByteArray::number(d->responseBody.length()) ) );

    QByteArray response = d->response.toString();
    response.replace(0,4,transport);    //TODO: #ak looks too bad. Add support for any protocol to nx_http (nx_http has to be renamed in this case)
    if (!d->responseBody.isEmpty())
    {
        response += d->responseBody;
    }
#else
    d->responseHeaders.setStatusLine(code, codeToMessage(code), d->requestHeaders.majorVersion(), d->requestHeaders.minorVersion());
    if (d->chunkedMode)
    {
        d->responseHeaders.setValue(QLatin1String("Transfer-Encoding"), QLatin1String("chunked"));
        d->responseHeaders.setContentType(QLatin1String(contentType));
    }

    if (!contentEncoding.isEmpty())
        d->responseHeaders.setValue(QLatin1String("Content-Encoding"), QLatin1String(contentEncoding));
    if (!contentType.isEmpty())
        d->responseHeaders.setValue(QLatin1String("Content-Type"), QLatin1String(contentType));
    if (!d->chunkedMode)
        d->responseHeaders.setValue(QLatin1String("Content-Length"), QString::number(d->responseBody.length()));

    QByteArray response = d->responseHeaders.toString().toUtf8();
    response.replace(0,4,transport);
    if (!d->responseBody.isEmpty())
    {
        response += d->responseBody;
    }
#endif

    if (displayDebug)
        NX_LOG(lit("Server response to %1:\n%2").arg(d->socket->getPeerAddress().address.toString()).arg(QString::fromLatin1(response)), cl_logDEBUG1);

    QMutexLocker lock(&d->sockMutex);
    sendData(response.data(), response.size());
}

bool QnTCPConnectionProcessor::sendChunk( const QnByteArray& chunk )
{
    return sendChunk( chunk.data(), chunk.size() );
}

bool QnTCPConnectionProcessor::sendChunk( const QByteArray& chunk )
{
    return sendChunk( chunk.data(), chunk.size() );
}

bool QnTCPConnectionProcessor::sendChunk( const char* data, int size )
{
    Q_D(QnTCPConnectionProcessor);
    QByteArray result = QByteArray::number(size,16);
    result.append("\r\n");
    result.append(data, size);  //TODO/IMPL avoid copying by implementing writev in socket
    result.append("\r\n");

    int sended;
    sended = d->socket->send(result);
    return sended == result.size();
}

QString QnTCPConnectionProcessor::codeToMessage(int code)
{
    switch(code)
    {
    case CODE_OK:
        return tr("OK");
    case CODE_NOT_FOUND:
        return tr("Not Found");
    case CODE_NOT_IMPLEMETED:
        return tr("Not Implemented");
    case CODE_UNSPOORTED_TRANSPORT:
        return tr("Unsupported Transport");
    case CODE_INTERNAL_ERROR:
        return tr("Internal Server Error");
    case CODE_INVALID_PARAMETER:
        return tr("Invalid Parameter");
    }
    return QString ();
}

void QnTCPConnectionProcessor::pleaseStop()
{
    Q_D(QnTCPConnectionProcessor);
    if (d->socket)
        d->socket->close();
    QnLongRunnable::pleaseStop();
}

QSharedPointer<AbstractStreamSocket> QnTCPConnectionProcessor::socket() const
{
    Q_D(const QnTCPConnectionProcessor);
    return d->socket;
}

int QnTCPConnectionProcessor::getSocketTimeout()
{
    Q_D(QnTCPConnectionProcessor);
    return d->socketTimeout;
}

int QnTCPConnectionProcessor::readSocket( quint8* buffer, int bufSize )
{
    Q_D(QnTCPConnectionProcessor);
    return d->socket->recv( buffer, bufSize );
}

bool QnTCPConnectionProcessor::readRequest()
{
    Q_D(QnTCPConnectionProcessor);

    //QElapsedTimer globalTimeout;
#ifdef USE_NX_HTTP
    d->request = nx_http::Request();
    d->response = nx_http::Response();
#else
    d->requestHeaders = QHttpRequestHeader();
    d->responseHeaders = QHttpResponseHeader();
#endif
    d->clientRequest.clear();
    d->requestBody.clear();
    d->responseBody.clear();

    while (!needToStop() && d->socket->isConnected())
    {
        int readed;
        readed = d->socket->recv(d->tcpReadBuffer, TCP_READ_BUFFER_SIZE);
        if (readed > 0) 
        {
            //globalTimeout.restart();
            d->clientRequest.append((const char*) d->tcpReadBuffer, readed);
            if (isFullMessage(d->clientRequest))
            {
                return true;
            }
            else if (d->clientRequest.size() > MAX_REQUEST_SIZE)
            {
                qWarning() << "Too large HTTP client request. Ignoring";
                return false;
            }
        }
        else //if (globalTimeout.elapsed() > CONNECTION_TIMEOUT)
        {
            break;
        }
    }
    return false;
}

void QnTCPConnectionProcessor::copyClientRequestTo(QnTCPConnectionProcessor& other)
{
    Q_D(const QnTCPConnectionProcessor);
    other.d_ptr->clientRequest = d->clientRequest;
    other.d_ptr->protocol = d->protocol;
}

QUrl QnTCPConnectionProcessor::getDecodedUrl() const
{
    Q_D(const QnTCPConnectionProcessor);
#ifdef USE_NX_HTTP
    return d->request.requestLine.url;
#else
    QByteArray data = d->requestHeaders.path().toUtf8();
    data = data.replace("+", "%20");
    return QUrl::fromEncoded(data);
#endif
}

void QnTCPConnectionProcessor::execute(QMutex& mutex)
{
    m_needStop = false;
    mutex.unlock();
    run();
    mutex.lock();
}

SocketAddress QnTCPConnectionProcessor::remoteHostAddress() const
{
    Q_D(const QnTCPConnectionProcessor);
    return d->socket ? d->socket->getPeerAddress() : SocketAddress();
}

void QnTCPConnectionProcessor::releaseSocket()
{
    Q_D(QnTCPConnectionProcessor);
    d->socket.clear();    
}
