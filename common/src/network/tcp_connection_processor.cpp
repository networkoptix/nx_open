#include "tcp_connection_processor.h"

#include <QtCore/QTime>

#include <nx/network/aio/unified_pollset.h>
#include <nx/network/flash_socket/types.h>
#include <nx/network/http/http_types.h>
#include <nx/network/http/http_mod_manager.h>
#include <nx/utils/log/log.h>
#include <nx/utils/gzip/gzip_compressor.h>

#include <utils/common/util.h>

#include "tcp_listener.h"
#include "tcp_connection_priv.h"

#ifndef Q_OS_WIN
#   include <netinet/tcp.h>
#endif
#include "core/resource_management/resource_pool.h"
#include <nx/network/http/custom_headers.h>
#include "common/common_module.h"

// we need enough size for updates
#ifdef __arm__
    static const int MAX_REQUEST_SIZE = 1024*1024*16;
#else
    static const int MAX_REQUEST_SIZE = 1024*1024*256;
#endif


QnTCPConnectionProcessor::QnTCPConnectionProcessor(
    QSharedPointer<AbstractStreamSocket> socket,
    QnTcpListener* owner)
:
    QnCommonModuleAware(owner->commonModule()),
    d_ptr(new QnTCPConnectionProcessorPrivate)
{
    Q_D(QnTCPConnectionProcessor);
    d->socket = socket;
    d->owner = owner;
}

QnTCPConnectionProcessor::QnTCPConnectionProcessor(
    QnTCPConnectionProcessorPrivate* dptr,
    QSharedPointer<AbstractStreamSocket> socket,
    QnTcpListener* owner)
:
    QnCommonModuleAware(owner->commonModule()),
    d_ptr(dptr)
{
    Q_D(QnTCPConnectionProcessor);
    d->socket = socket;
    d->owner = owner;
}

QnTCPConnectionProcessor::QnTCPConnectionProcessor(
    QnTCPConnectionProcessorPrivate* dptr,
    QSharedPointer<AbstractStreamSocket> socket,
    QnCommonModule* commonModule)
:
    QnCommonModuleAware(commonModule),
    d_ptr(dptr)
{
    Q_D(QnTCPConnectionProcessor);
    d->socket = socket;
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

        bool isOk = false;
        if (posStart >= 0)
            contentLen = lRequest.mid(posStart, posEnd - posStart+1).toInt(&isOk);

        if (!isOk || contentLen < 0)
            return -1;
    }

    QByteArray dblDelim = delimiter + delimiter;
    const int dblDelimPos = message.indexOf(dblDelim);
    if (dblDelimPos == -1)
        return 0;

    const int expectedSize = dblDelimPos + dblDelim.size() + contentLen;
    if (expectedSize < dblDelimPos + dblDelim.size())
        return -1;

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
    if (d->owner)
        d->owner->applyModToRequest(&d->request);
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

    QnMutexLocker lock( &d->sockMutex );
    return sendData(sendBuffer.data(), sendBuffer.size());
}

bool QnTCPConnectionProcessor::sendBuffer(const QByteArray& sendBuffer)
{
    Q_D(QnTCPConnectionProcessor);

    QnMutexLocker lock( &d->sockMutex );
    return sendData(sendBuffer.data(), sendBuffer.size());
}

bool QnTCPConnectionProcessor::sendData(const char* data, int size)
{
    Q_D(QnTCPConnectionProcessor);
    while (!needToStop() && size > 0 && d->socket->isConnected())
    {
        int sended = 0;
        sended = d->socket->send(data, size);

        if (sended < 0 && SystemError::getLastOSErrorCode() == SystemError::wouldBlock)
        {
            unsigned int sendTimeout = 0;
            if (!d->socket->getSendTimeout(&sendTimeout))
                return false;
            const std::chrono::milliseconds kPollTimeout = std::chrono::milliseconds(sendTimeout);
            nx::network::aio::UnifiedPollSet pollSet;
            if (!pollSet.add(d->socket->pollable(), nx::network::aio::etWrite))
                return false;
            if (pollSet.poll(kPollTimeout) < 1)
                return false;
            continue; //< socket in async mode
        }

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

QByteArray QnTCPConnectionProcessor::createResponse(int httpStatusCode, const QByteArray& contentType, const QByteArray& contentEncoding, const QByteArray& multipartBoundary, bool displayDebug)
{
    Q_D(QnTCPConnectionProcessor);

    d->response.statusLine.version = d->request.requestLine.version;
    d->response.statusLine.statusCode = httpStatusCode;
    d->response.statusLine.reasonPhrase = nx_http::StatusCode::toString((nx_http::StatusCode::Value)httpStatusCode);

    if (d->response.headers.find("Connection") == d->response.headers.end() &&  //response does not contain
        isConnectionCanBePersistent())
    {
        d->response.headers.insert(nx_http::HttpHeader("Connection", "Keep-Alive"));
        if (d->response.headers.find("Keep-Alive") == d->response.headers.end())
            d->response.headers.insert(nx_http::HttpHeader("Keep-Alive", lit("timeout=%1").arg(KEEP_ALIVE_TIMEOUT/1000).toLatin1()) );
    }

    if (d->authenticatedOnce)
    {
        //revealing Server name to authenticated entity only
        nx_http::insertOrReplaceHeader(
            &d->response.headers,
            nx_http::HttpHeader(nx_http::header::Server::NAME, nx_http::serverString() ) );
    }
    nx_http::insertOrReplaceHeader(
        &d->response.headers,
        nx_http::HttpHeader("Date", nx_http::formatDateTime(QDateTime::currentDateTime())) );

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

    return response;
}

void QnTCPConnectionProcessor::sendResponse(int httpStatusCode, const QByteArray& contentType, const QByteArray& contentEncoding, const QByteArray& multipartBoundary, bool displayDebug)
{
    Q_D(QnTCPConnectionProcessor);
    auto response = createResponse(httpStatusCode, contentType, contentEncoding, multipartBoundary, displayDebug);
    QnMutexLocker lock(&d->sockMutex);
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
    if (auto socket = d->socket)
        socket->shutdown();
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
            const auto messageSize = isFullMessage(d->clientRequest, &fullHttpMessageSize);
            if (messageSize < 0)
                return false;

            if (messageSize)
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
        NX_ASSERT( d->interleavedMessageData.isEmpty() );
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

            if (d->owner)
                d->owner->applyModToRequest(&d->request);

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
    other.d_ptr->request = d->request;
    other.d_ptr->response = d->response;
    other.d_ptr->protocol = d->protocol;
    other.d_ptr->accessRights = d->accessRights;
    other.d_ptr->authenticatedOnce = d->authenticatedOnce;
}

QUrl QnTCPConnectionProcessor::getDecodedUrl() const
{
    Q_D(const QnTCPConnectionProcessor);
    return d->request.requestLine.url;
}

void QnTCPConnectionProcessor::execute(QnMutex& mutex)
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

bool QnTCPConnectionProcessor::isSocketTaken() const
{
    Q_D(const QnTCPConnectionProcessor);
    return d->isSocketTaken;
}

QSharedPointer<AbstractStreamSocket> QnTCPConnectionProcessor::takeSocket()
{
    Q_D(QnTCPConnectionProcessor);
    d->isSocketTaken = true;

    const auto socket = d->socket;
    d->socket.clear();
    return socket;
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

int QnTCPConnectionProcessor::notFound(QByteArray& contentType)
{
    Q_D(QnTCPConnectionProcessor);
    contentType = "text/html; charset=utf-8";
    d->response.messageBody = "<html><head><title>Not Found</title></head><body><h1>Not Found</h1></html>";
    return CODE_NOT_FOUND;
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

    QByteArray existSession = nx_http::getHeaderValue(d->request.headers, Qn::AUTH_SESSION_HEADER_NAME);
    if (!existSession.isEmpty()) {
        result.fromByteArray(existSession);
        return result;
    }
    if (const auto& userRes = resourcePool()->getResourceById(d->accessRights.userId))
        result.userName = userRes->getName();
    else if (!nx_http::getHeaderValue( d->request.headers,  Qn::VIDEOWALL_GUID_HEADER_NAME).isEmpty())
        result.userName = lit("Video wall");

    result.id = QnUuid::fromStringSafe(nx_http::getHeaderValue(d->request.headers, Qn::EC2_RUNTIME_GUID_HEADER_NAME));
    if (result.id.isNull())
        result.id = QnUuid::fromStringSafe(d->request.getCookieValue(Qn::EC2_RUNTIME_GUID_HEADER_NAME));
    const QUrlQuery query(d->request.requestLine.url.query());
    if (result.id.isNull())
        result.id = QnUuid::fromStringSafe(query.queryItemValue(QLatin1String(Qn::EC2_RUNTIME_GUID_HEADER_NAME)));
    if (result.id.isNull())
    {
        QByteArray nonce = d->request.getCookieValue("auth");
        if (!nonce.isEmpty())
        {
            QCryptographicHash md5Hash( QCryptographicHash::Md5 );
            md5Hash.addData( nonce );
            result.id = QnUuid::fromRfc4122(md5Hash.result());
        }
    }
    if (result.id.isNull())
        result.id = QnUuid::createUuid();

    result.userHost = QString::fromUtf8(nx_http::getHeaderValue(d->request.headers, Qn::USER_HOST_HEADER_NAME));
    if (result.userHost.isEmpty())
        result.userHost = d->socket->getForeignAddress().address.toString();
    result.userAgent = query.queryItemValue(QLatin1String(Qn::USER_AGENT_HEADER_NAME));
    if (result.userAgent.isEmpty())
        result.userAgent = QString::fromUtf8(nx_http::getHeaderValue(d->request.headers, Qn::USER_AGENT_HEADER_NAME));

    int trimmedPos = result.userAgent.indexOf(lit("/"));
    if (trimmedPos != -1) {
        trimmedPos = result.userAgent.indexOf(lit(" "), trimmedPos);
        result.userAgent = result.userAgent.left(trimmedPos);
    }

    return result;
}

void QnTCPConnectionProcessor::sendUnauthorizedResponse(nx_http::StatusCode::Value httpResult, const QByteArray& messageBody)
{
    Q_D(QnTCPConnectionProcessor);

    if( d->request.requestLine.method == nx_http::Method::get ||
        d->request.requestLine.method == nx_http::Method::head )
    {
        d->response.messageBody = messageBody;
    }
    if (nx_http::getHeaderValue( d->response.headers, Qn::SERVER_GUID_HEADER_NAME ).isEmpty())
        d->response.headers.insert(nx_http::HttpHeader(Qn::SERVER_GUID_HEADER_NAME, commonModule()->moduleGUID().toByteArray()));

    auto acceptEncodingHeaderIter = d->request.headers.find( "Accept-Encoding" );
    QByteArray contentEncoding;
    if( acceptEncodingHeaderIter != d->request.headers.end() )
    {
        nx_http::header::AcceptEncodingHeader acceptEncodingHeader( acceptEncodingHeaderIter->second );
        if( acceptEncodingHeader.encodingIsAllowed( "identity" ) )
        {
            contentEncoding = "identity";
        }
        else if( acceptEncodingHeader.encodingIsAllowed( "gzip" ) )
        {
            contentEncoding = "gzip";
            if( !d->response.messageBody.isEmpty() )
                d->response.messageBody = nx::utils::bstream::gzip::Compressor::compressData(d->response.messageBody);
        }
        else
        {
            //TODO #ak not supported encoding requested
        }
    }
    sendResponse(
        httpResult,
        d->response.messageBody.isEmpty() ? QByteArray() : "text/html; charset=utf-8",
        contentEncoding );
}
