#include "tcp_connection_processor.h"

#include <QtCore/QTime>

#include <utils/common/log.h>
#include <utils/common/util.h>
#include <utils/network/flash_socket/types.h>
#include <utils/network/http/httptypes.h>

#include "http/http_mod_manager.h"
#include "tcp_listener.h"
#include "tcp_connection_priv.h"
#include "err.h"

#include "version.h"


#ifndef Q_OS_WIN
#   include <netinet/tcp.h>
#endif
#include "core/resource_management/resource_pool.h"
#include "http/custom_headers.h"

// we need enough size for updates
#ifdef __arm__
    static const int MAX_REQUEST_SIZE = 1024*1024*16;
#else
    static const int MAX_REQUEST_SIZE = 1024*1024*256;
#endif


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

int QnTCPConnectionProcessor::isFullMessage(
    const QByteArray& message,
    boost::optional<qint64>* const fullMessageSize )
{
    if( message.startsWith(nx_flash_sock::POLICY_FILE_REQUEST_NAME) )
        return message.size();

    if( fullMessageSize && fullMessageSize->is_initialized() )
        return fullMessageSize->get() > message.size()
            ? 0
            : fullMessageSize->get();   //signalling that full message has been read

    QByteArray delimiter = "\n";
    int pos = message.indexOf(delimiter);
    if (pos == -1)
        return 0;
    if (pos > 0 && message[pos-1] == '\r')
        delimiter = "\r\n";
    int contentLen = 0;

    //retrieving content-length from message
    QByteArray lRequest = message.toLower();
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
    const int dblDelimPos = message.indexOf(dblDelim);
    if (dblDelimPos == -1)
        return 0;
    const int expectedSize = dblDelimPos + dblDelim.size() + contentLen;
    if( fullMessageSize )
        *fullMessageSize = expectedSize;
    if (expectedSize > message.size())
        return 0;
    else 
        return expectedSize;
}

void QnTCPConnectionProcessor::parseRequest()
{
    Q_D(QnTCPConnectionProcessor);
//    qDebug() << "Client request from " << d->socket->getForeignAddress().address.toString();
//    qDebug() << d->clientRequest;

    d->request = nx_http::Request();
    if( !d->request.parse( d->clientRequest ) )
    {
        qWarning() << Q_FUNC_INFO << "Invalid request format.";
        return;
    }
    d->protocol = d->request.requestLine.version.protocol;
    d->requestBody = d->request.messageBody;

    nx_http::HttpModManager::instance()->apply( &d->request );
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
        data += sended;
        size -= sended;
    }
    return d->socket->isConnected();
}


QString QnTCPConnectionProcessor::extractPath() const
{
    Q_D(const QnTCPConnectionProcessor);
    return d->request.requestLine.url.path();
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

void QnTCPConnectionProcessor::sendResponse(int httpStatusCode, const QByteArray& contentType, const QByteArray& contentEncoding, const QByteArray& multipartBoundary, bool displayDebug)
{
    Q_D(QnTCPConnectionProcessor);

    d->response.statusLine.version = d->request.requestLine.version;
    d->response.statusLine.statusCode = httpStatusCode;
    d->response.statusLine.reasonPhrase = nx_http::StatusCode::toString((nx_http::StatusCode::Value)httpStatusCode);

    if( isConnectionCanBePersistent() )
    {
        d->response.headers.insert(nx_http::HttpHeader("Connection", "Keep-Alive"));
        d->response.headers.insert(nx_http::HttpHeader("Keep-Alive", lit("timeout=%1, max=%1").arg(KEEP_ALIVE_TIMEOUT/1000).toLatin1()) );
    }

    nx_http::insertOrReplaceHeader(
        &d->response.headers,
        nx_http::HttpHeader("Server", nx_http::serverString() ) );
    nx_http::insertOrReplaceHeader(
        &d->response.headers,
        nx_http::HttpHeader("Date", dateTimeToHTTPFormat(QDateTime::currentDateTime())) );

    // this header required to perform new HTTP requests if server port has been on the fly changed
    nx_http::insertOrReplaceHeader( &d->response.headers, nx_http::HttpHeader( "Access-Control-Allow-Origin", "*" ) );

    if (d->chunkedMode)
        nx_http::insertOrReplaceHeader( &d->response.headers, nx_http::HttpHeader( "Transfer-Encoding", "chunked" ) );

    if (!contentEncoding.isEmpty() && contentEncoding != "identity")
        nx_http::insertOrReplaceHeader( &d->response.headers, nx_http::HttpHeader( "Content-Encoding", contentEncoding ) );
    if (!contentType.isEmpty())
        nx_http::insertOrReplaceHeader( &d->response.headers, nx_http::HttpHeader( "Content-Type", contentType ) );
    if (!d->chunkedMode /*&& !contentType.isEmpty()*/ && (contentType.indexOf("multipart") == -1))
        nx_http::insertOrReplaceHeader( &d->response.headers, nx_http::HttpHeader( "Content-Length", QByteArray::number(d->response.messageBody.length()) ) );

    QByteArray response = !multipartBoundary.isEmpty() ? d->response.toMultipartString(multipartBoundary) : d->response.toString();

    if (displayDebug)
        NX_LOG(lit("Server response to %1:\n%2").arg(d->socket->getForeignAddress().address.toString()).arg(QString::fromLatin1(response)), cl_logDEBUG1);

    NX_LOG( QnLog::HTTP_LOG_INDEX, QString::fromLatin1("Sending response to %1:\n%2\n-------------------\n\n\n").
        arg(d->socket->getForeignAddress().toString()).
        arg(QString::fromLatin1(QByteArray::fromRawData(response.constData(), response.size() - (!contentEncoding.isEmpty() ? d->response.messageBody.size() : 0)))), cl_logDEBUG1 );

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
    QByteArray result = QByteArray::number(size,16);
    result.append("\r\n");
    result.append(data, size);  //TODO/IMPL avoid copying by implementing writev in socket
    result.append("\r\n");

    return sendData( result.constData(), result.size() );
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
    d->request = nx_http::Request();
    d->response = nx_http::Response();
    d->clientRequest.clear();
    d->requestBody.clear();

    boost::optional<qint64> fullHttpMessageSize;
    while (!needToStop() && d->socket->isConnected())
    {
        int readed;
        readed = d->socket->recv(d->tcpReadBuffer, TCP_READ_BUFFER_SIZE);
        if (readed > 0) 
        {
            //globalTimeout.restart();
            d->clientRequest.append((const char*) d->tcpReadBuffer, readed);
            if (isFullMessage(d->clientRequest, &fullHttpMessageSize))
            {
                NX_LOG( QnLog::HTTP_LOG_INDEX, QString::fromLatin1("Received request from %1:\n%2-------------------\n\n\n").
                    arg(d->socket->getForeignAddress().toString()).
                    arg(QString::fromLatin1(d->clientRequest)), cl_logDEBUG1 );
                return true;
            }
            else if (d->clientRequest.size() > MAX_REQUEST_SIZE)
            {
                qWarning() << "Too large HTTP client request ("<<d->clientRequest.size()<<" bytes"
                    ", "<<MAX_REQUEST_SIZE<<" allowed). Ignoring...";
                return false;
            }
            if( fullHttpMessageSize )
                d->clientRequest.reserve( fullHttpMessageSize.get() );
        }
        else //if (globalTimeout.elapsed() > CONNECTION_TIMEOUT)
        {
            break;
        }
    }

    return false;
}

bool QnTCPConnectionProcessor::readSingleRequest()
{
    Q_D(QnTCPConnectionProcessor);

    d->request = nx_http::Request();
    d->response = nx_http::Response();
    d->requestBody.clear();
    d->currentRequestSize = 0;
    d->prevSocketError = SystemError::noError;

    if( !d->clientRequest.isEmpty() )   //TODO #ak it is more reliable to check for the first call of this method
    {
        //due to bug in QnTCPConnectionProcessor::readRequest() d->clientRequest 
        //    can contain multiple interleaved requests.
        //    Have to parse them!
        assert( d->interleavedMessageData.isEmpty() );
        d->interleavedMessageData = d->clientRequest;    //no copying here!
        d->clientRequest.clear();
        d->interleavedMessageDataPos = 0;
    }

    while (!needToStop() && d->socket->isConnected())
    {
        if( d->interleavedMessageDataPos == (size_t)d->interleavedMessageData.size() )
        {
            //buffer depleted, draining more data
            const int readed = d->socket->recv(d->tcpReadBuffer, TCP_READ_BUFFER_SIZE);
            if (readed <= 0)
            {
                d->prevSocketError = SystemError::getLastOSErrorCode();
                NX_LOG( lit("Error reading request from %1: %2").
                    arg(d->socket->getForeignAddress().toString()).arg(SystemError::toString( d->prevSocketError )),
                    cl_logDEBUG1 );
                return false;
            }
            d->interleavedMessageData = QByteArray::fromRawData( (const char*)d->tcpReadBuffer, readed );
            d->interleavedMessageDataPos = 0;
        }

        size_t bytesParsed = 0;
        if( !d->httpStreamReader.parseBytes(
                QnByteArrayConstRef( d->interleavedMessageData, d->interleavedMessageDataPos ),
                &bytesParsed ) ||
            (d->httpStreamReader.state() == nx_http::HttpStreamReader::parseError) )
        {
            //parse error
            return false;
        }
        d->currentRequestSize += bytesParsed;
        d->interleavedMessageDataPos += bytesParsed;

        if( d->currentRequestSize > MAX_REQUEST_SIZE )
        {
            qWarning() << "Too large HTTP client request ("<<d->currentRequestSize<<" bytes"
                ", "<<MAX_REQUEST_SIZE<<" allowed). Ignoring...";
            return false;
        }

        if( d->httpStreamReader.state() == nx_http::HttpStreamReader::messageDone )
        {
            if( d->httpStreamReader.message().type != nx_http::MessageType::request )
                return false;
            //TODO #ak we have parsed message in d->httpStreamReader: should use it, not copy!
            d->request = *d->httpStreamReader.message().request;
            d->protocol = d->request.requestLine.version.protocol;
            d->requestBody = d->httpStreamReader.fetchMessageBody();

            //TODO #ak logging
            //NX_LOG( QnLog::HTTP_LOG_INDEX, QString::fromLatin1("Received request from %1:\n%2-------------------\n\n\n").
            //    arg(d->socket->getForeignAddress().toString()).
            //    arg(QString::fromLatin1(d->clientRequest)), cl_logDEBUG1 );
            return true;
        }
    }
    return false;
}

void QnTCPConnectionProcessor::copyClientRequestTo(QnTCPConnectionProcessor& other)
{
    Q_D(const QnTCPConnectionProcessor);
    other.d_ptr->clientRequest = d->clientRequest;
    other.d_ptr->protocol = d->protocol;
    other.d_ptr->authUserId = d->authUserId;
}

QUrl QnTCPConnectionProcessor::getDecodedUrl() const
{
    Q_D(const QnTCPConnectionProcessor);
    return d->request.requestLine.url;
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
    return d->socket ? d->socket->getForeignAddress() : SocketAddress();
}

void QnTCPConnectionProcessor::releaseSocket()
{
    Q_D(QnTCPConnectionProcessor);
    d->socket.clear();    
}

int QnTCPConnectionProcessor::redirectTo(const QByteArray& page, QByteArray& contentType)
{
    Q_D(QnTCPConnectionProcessor);
    contentType = "text/html; charset=utf-8";
    d->response.messageBody = "<html><head><title>Moved</title></head><body><h1>Moved</h1></html>";
    d->response.headers.insert(nx_http::HttpHeader("Location", page));
    return CODE_MOVED_PERMANENTLY;
}

bool QnTCPConnectionProcessor::isConnectionCanBePersistent() const
{
    Q_D(const QnTCPConnectionProcessor);

    if( d->request.requestLine.version == nx_http::http_1_1 )
        return nx_http::getHeaderValue( d->request.headers, "Connection" ).toLower() != "close";
    else if( d->request.requestLine.version == nx_http::http_1_0 )
        return nx_http::getHeaderValue( d->request.headers, "Connection" ).toLower() == "keep-alive";
    else    //e.g., RTSP
        return false;
}

QnAuthSession QnTCPConnectionProcessor::authSession() const
{
    Q_D(const QnTCPConnectionProcessor);
    QnAuthSession result;
    if (const auto& userRes = qnResPool->getResourceById(d->authUserId))
        result.userName = userRes->getName();
    result.sessionId = nx_http::getHeaderValue(d->request.headers, Qn::EC2_RUNTIME_GUID_HEADER_NAME);
    if (result.sessionId.isNull())
        result.sessionId = d->request.getCookieValue(Qn::EC2_RUNTIME_GUID_HEADER_NAME);
    if (result.sessionId.isNull()) {
        QUrlQuery query(d->request.requestLine.url.query());
        result.sessionId = query.queryItemValue(QLatin1String(Qn::EC2_RUNTIME_GUID_HEADER_NAME));
    }
    if (result.sessionId.isNull())
        result.sessionId = QnUuid::createUuid();

    result.userHost = d->socket->getForeignAddress().address.toString();
    result.userAgent = QString::fromUtf8(nx_http::getHeaderValue(d->request.headers, "User-Agent"));

    int trimmedPos = result.userAgent.indexOf(lit("/"));
    if (trimmedPos != -1) {
        trimmedPos = result.userAgent.indexOf(lit(" "), trimmedPos);
        result.userAgent = result.userAgent.left(trimmedPos);
    }

    return result;
}
