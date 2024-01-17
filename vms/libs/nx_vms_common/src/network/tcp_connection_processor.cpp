// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "tcp_connection_processor.h"

#include <chrono>
#include <optional>

#ifndef Q_OS_WIN
    #include <netinet/tcp.h>
#endif

#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/metrics/metrics_storage.h>
#include <nx/network/aio/unified_pollset.h>
#include <nx/network/flash_socket/types.h>
#include <nx/network/http/custom_headers.h>
#include <nx/network/http/http_types.h>
#include <nx/network/rest/result.h>
#include <nx/network/stun/message.h>
#include <nx/utils/gzip/gzip_compressor.h>
#include <nx/utils/log/log.h>
#include <utils/common/util.h>

#include "tcp_connection_priv.h"
#include "tcp_listener.h"

namespace {

const qint64 kLargeTcpRequestSize = 4 * 1024 * 1024 * 1024ll;

void copyAndReplaceHeader(
    const nx::network::http::HttpHeaders* srcHeaders,
    nx::network::http::HttpHeaders* dstHeaders,
    const char* headerName)
{
    auto headerItr = srcHeaders->find(headerName);
    if (headerItr == srcHeaders->end())
        return;
    nx::network::http::insertOrReplaceHeader(
        dstHeaders, nx::network::http::HttpHeader(headerName, headerItr->second));
}

} // namespace

QnTCPConnectionProcessor::QnTCPConnectionProcessor(
    std::unique_ptr<nx::network::AbstractStreamSocket> socket,
    QnTcpListener* owner,
    int maxTcpRequestSize)
    :
    nx::vms::common::SystemContextAware(owner->systemContext()),
    d_ptr(new QnTCPConnectionProcessorPrivate),
    m_maxTcpRequestSize(maxTcpRequestSize)
{
    Q_D(QnTCPConnectionProcessor);
    d->socket = std::move(socket);
    d->owner = owner;
}

QnTCPConnectionProcessor::QnTCPConnectionProcessor(
    QnTCPConnectionProcessorPrivate* dptr,
    std::unique_ptr<nx::network::AbstractStreamSocket> socket,
    QnTcpListener* owner,
    int maxTcpRequestSize)
    :
    nx::vms::common::SystemContextAware(owner->systemContext()),
    d_ptr(dptr),
    m_maxTcpRequestSize(maxTcpRequestSize)
{
    Q_D(QnTCPConnectionProcessor);
    d->socket = std::move(socket);
    d->owner = owner;
}

QnTCPConnectionProcessor::QnTCPConnectionProcessor(
    QnTCPConnectionProcessorPrivate* dptr,
    std::unique_ptr<nx::network::AbstractStreamSocket> socket,
    nx::vms::common::SystemContext* systemContext,
    int maxTcpRequestSize)
    :
    nx::vms::common::SystemContextAware(systemContext),
    d_ptr(dptr),
    m_maxTcpRequestSize(maxTcpRequestSize)
{
    Q_D(QnTCPConnectionProcessor);
    d->socket = std::move(socket);
}

void QnTCPConnectionProcessor::stop()
{
    base_type::stop();
}

QnTCPConnectionProcessor::~QnTCPConnectionProcessor()
{
    stop();
    delete d_ptr;
}

int QnTCPConnectionProcessor::isFullMessage(
    const QByteArray& message,
    std::optional<qint64>* const fullMessageSize )
{
    if( message.startsWith(nx_flash_sock::POLICY_FILE_REQUEST_NAME) )
        return message.size();

    if( fullMessageSize && fullMessageSize->has_value() )
        return fullMessageSize->value() > message.size()
            ? 0
            : fullMessageSize->value();   //signalling that full message has been read

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

int QnTCPConnectionProcessor::isFullMessage(
    const nx::Buffer& message,
    std::optional<qint64>* const fullMessageSize)
{
    return isFullMessage(message.toRawByteArray(), fullMessageSize);
}

bool QnTCPConnectionProcessor::isLargeRequestAllowed(const nx::utils::Url& url)
{
    Q_D(QnTCPConnectionProcessor);
    return d->owner->isLargeRequestAllowed(url.path());
}

bool QnTCPConnectionProcessor::isLargeRequestAllowed(const QByteArray& message)
{
    int pos = message.indexOf('\n');
    if (pos == -1)
        return false;
    if (pos > 0 && message[pos-1] == '\r')
        pos--;

    nx::network::http::RequestLine line;
    if (!line.parse(QByteArray(message.data(), pos)))
        return false;

    return isLargeRequestAllowed(line.url);
}

int QnTCPConnectionProcessor::checkForBinaryProtocol(const QByteArray& message)
{
    Q_D(QnTCPConnectionProcessor);
    if (message.size() > 10 && message[0] == 0x0 && message[2] == 0x0 && message[3] == 0x1
        && qFromBigEndian(*(uint32_t*) &message.data()[6]) == nx::network::stun::MAGIC_COOKIE)
    {
        /* By rfc4571, first TWO bytes should be a length.
         * But, for STUN content, first message is Binding Request,
         * which very likely be less then 256 bytes length
         */
        d->protocol = kRfc4571Stun;
        d->binaryProtocol = true;
        int size = message[1];
        return size;
    }
    return -1;
}

void QnTCPConnectionProcessor::parseRequest()
{
    Q_D(QnTCPConnectionProcessor);

    d->request = nx::network::http::Request();

    if (isBinaryProtocol())
    {
        return;
    }
    else if (!d->request.parse(d->clientRequest))
    {
        NX_DEBUG(this, "Unable to parse request: [%1]", d->clientRequest);
        return;
    }
    d->protocol = d->request.requestLine.version.protocol;
    d->requestBody = d->request.messageBody.toByteArray();
    if (d->owner)
        d->owner->applyModToRequest(&d->request);

    if (!d->requestLogged)
    {
        d->requestLogged = true;
        logRequestOrResponse(
            "Received request from",
            QByteArray::fromStdString(nx::network::http::getHeaderValue(d->request.headers, "Content-Type")),
            QByteArray::fromStdString(nx::network::http::getHeaderValue(d->request.headers, "Content-Encoding")),
            d->clientRequest,
            d->request.messageBody);
    }
}

bool QnTCPConnectionProcessor::sendBuffer(
    const nx::utils::ByteArray& sendBuffer, std::optional<int64_t> timestampForLogging)
{
    return sendBufferThreadSafe(
        sendBuffer.constData(), (int) sendBuffer.size(), timestampForLogging);
}

bool QnTCPConnectionProcessor::sendBuffer(
    const QByteArray& sendBuffer, std::optional<int64_t> timestampForLogging)
{
    return sendBufferThreadSafe(
        sendBuffer.constData(), sendBuffer.size(), timestampForLogging);
}

bool QnTCPConnectionProcessor::sendBuffer(
    const char* data, int size, std::optional<int64_t> timestampForLogging)
{
    return sendBufferThreadSafe(data, size, timestampForLogging);
}

bool QnTCPConnectionProcessor::sendBufferThreadSafe(
    const char* data,
    int size,
    std::optional<int64_t> timestampForLogging)
{
    Q_D(QnTCPConnectionProcessor);

    NX_MUTEX_LOCKER lock(&d->sendDataMutex);

    while (!needToStop() && size > 0 && d->socket->isConnected())
    {
        const auto sendDataResult = d->sendData(data, size, timestampForLogging);

        if (sendDataResult.sendResult == 0)
        {
            NX_DEBUG(this, "Socket was closed by the other peer (send() -> 0).");
            return false;
        }

        if (sendDataResult.sendResult < 0)
        {
            if (sendDataResult.errorCode == SystemError::interrupted)
                continue; //< Retrying interrupted call.

            if (d->socket->isConnected() &&
                (sendDataResult.errorCode == SystemError::wouldBlock
                || sendDataResult.errorCode == SystemError::again))
            {
                unsigned int sendTimeout = 0;
                if (!d->socket->getSendTimeout(&sendTimeout))
                {
                    NX_WARNING(this, "Unable to get socket timeout.");
                    return false;
                }
                const std::chrono::milliseconds kPollTimeout =
                    std::chrono::milliseconds(sendTimeout);
                nx::network::aio::UnifiedPollSet pollSet;
                if (!pollSet.add(d->socket->pollable(), nx::network::aio::etWrite))
                {
                    NX_WARNING(this, "Unable to start polling the socket.");
                    return false;
                }
                if (pollSet.poll(kPollTimeout) < 1)
                {
                    NX_WARNING(this, "Unable to poll the socket.");
                    return false;
                }
                continue; //< socket in async mode
            }

            NX_DEBUG(this, "Unable to send data to socket: %1 (OS code %2).",
                SystemError::toString(sendDataResult.errorCode), sendDataResult.errorCode);
            return false;
        }

        data += sendDataResult.sendResult;
        size -= sendDataResult.sendResult;
    }

    if (!d->socket->isConnected())
    {
        NX_DEBUG(this, "Socket was disconnected.");
        return false;
    }

    return true;
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

std::pair<nx::String, nx::String> QnTCPConnectionProcessor::generateErrorResponse(
    nx::network::http::StatusCode::Value errorCode, const nx::String& details) const
{
    Q_D(const QnTCPConnectionProcessor);
    using namespace nx::network::http;
    const auto hasJsonInHeader =
        [d](const auto& name)
        {
            const auto value = getHeaderValue(d->request.headers, name);
            return nx::utils::contains(value, header::ContentType::kJson.value);
        };
    if (hasJsonInHeader(header::kAccept) || hasJsonInHeader(header::kContentType))
    {
        return std::pair<nx::String, nx::String>(
            header::ContentType::kJson.toString(),
            QJson::serialized(nx::network::rest::Result{
                nx::network::rest::Result::errorFromHttpStatus(errorCode), details}));
    }
    else
    {
        static const nx::String kAuthEnumPrefix = "Auth_";
        return std::pair<nx::String, nx::String>(
            header::ContentType::kHtml.toString(),
            NX_FMT(R"http(<HTML><HEAD><TITLE>%1</TITLE></HEAD><BODY><H1>%2 %3 -- %4</H1></BODY>)http",
                errorCode, static_cast<int>(errorCode), errorCode,
                details.startsWith(kAuthEnumPrefix)
                    ? details.mid(kAuthEnumPrefix.size())
                    : nx::String(details)));
    }
}

void QnTCPConnectionProcessor::logRequestOrResponse(
    const QByteArray& logMessage,
    const QByteArray& contentType,
    const QByteArray& contentEncoding,
    const QByteArray& httpMessageFull,
    const nx::Buffer& httpMessageBodyOnly)
{
    if (!nx::utils::log::isToBeLogged(nx::utils::log::Level::debug, nx::log::kHttpTag))
        return;

    Q_D(QnTCPConnectionProcessor);
    const auto uncompressDataIfNeeded =
        [&]()
        {
            return !(contentEncoding.isEmpty() || contentEncoding == "identity")
                ? nx::utils::bstream::gzip::Compressor::uncompressData(httpMessageBodyOnly).toByteArray()
                : httpMessageBodyOnly.toByteArray();
        };

    QByteArray bodyToPrint;
    if (contentType.startsWith("image/") || contentType.startsWith("audio/")
        || contentType.startsWith("video/") || contentType.contains("zip"))
    {
        // Do not print out media.
    }
    else if (contentType == "application/octet-stream" || contentType == "application/ubjson"
        || contentType == "application/x-periods")
    {
        bodyToPrint = uncompressDataIfNeeded().toHex();
    }
    else
    {
        bodyToPrint = uncompressDataIfNeeded();
    }

    if (!nx::utils::log::isToBeLogged(nx::utils::log::Level::verbose, nx::log::kHttpTag))
        bodyToPrint.truncate(1024 * 5); //< Should be enough for 5 devices.

    const auto headers = QByteArray::fromRawData(
        httpMessageFull.constData(), httpMessageFull.size() - httpMessageBodyOnly.size());

    NX_DEBUG(nx::log::kHttpTag, "%1 %2:\n%3%4%5",
        logMessage, d->socket->getForeignAddress(), headers,
        bodyToPrint, bodyToPrint.isEmpty() ? "" : "\n");
}

nx::String QnTCPConnectionProcessor::createResponse(
    int httpStatusCode,
    const nx::String& contentType,
    const nx::String& contentEncoding,
    const QByteArray& multipartBoundary,
    bool isUndefinedContentLength)
{
    Q_D(QnTCPConnectionProcessor);
    d->response.statusLine.version = d->request.requestLine.version;
    d->response.statusLine.statusCode =
        static_cast<nx::network::http::StatusCode::Value>(httpStatusCode);
    d->response.statusLine.reasonPhrase = nx::network::http::StatusCode::toString((nx::network::http::StatusCode::Value)httpStatusCode);

    if (d->response.headers.find("Connection") == d->response.headers.end() &&  //response does not contain
        isConnectionCanBePersistent())
    {
        d->response.headers.insert(nx::network::http::HttpHeader("Connection", "Keep-Alive"));
        if (d->response.headers.find("Keep-Alive") == d->response.headers.end())
            d->response.headers.insert(nx::network::http::HttpHeader("Keep-Alive", lit("timeout=%1").arg(KEEP_ALIVE_TIMEOUT/1000).toLatin1()) );
    }

    if (d->authenticatedOnce)
    {
        //revealing Server name to authenticated entity only
        nx::network::http::insertOrReplaceHeader(
            &d->response.headers,
            nx::network::http::HttpHeader(nx::network::http::header::Server::NAME, nx::network::http::serverString() ) );
    }
    nx::network::http::insertOrReplaceHeader(
        &d->response.headers,
        nx::network::http::HttpHeader("Date", nx::network::http::formatDateTime(QDateTime::currentDateTime())) );

    if (d->request.requestLine.url.scheme().startsWith("rtsp"))
        copyAndReplaceHeader(&d->request.headers, &d->response.headers, "CSeq");

    // this header required to perform new HTTP requests if server port has been on the fly changed
    if (d->response.headers.find("Access-Control-Allow-Origin") == d->response.headers.end())
    {
        nx::network::http::insertOrReplaceHeader(
            &d->response.headers,
            nx::network::http::HttpHeader("Access-Control-Allow-Origin", "*"));
    }

    if (d->chunkedMode)
        nx::network::http::insertOrReplaceHeader( &d->response.headers, nx::network::http::HttpHeader( "Transfer-Encoding", "chunked" ) );

    if (!contentEncoding.isEmpty() && contentEncoding != "identity")
        nx::network::http::insertOrReplaceHeader( &d->response.headers, nx::network::http::HttpHeader( "Content-Encoding", contentEncoding ) );
    if (!contentType.isEmpty())
        nx::network::http::insertOrReplaceHeader( &d->response.headers, nx::network::http::HttpHeader( "Content-Type", contentType ) );
    if (!isUndefinedContentLength &&!d->chunkedMode /*&& !contentType.isEmpty()*/ && (contentType.indexOf("multipart") == -1))
        nx::network::http::insertOrReplaceHeader( &d->response.headers, nx::network::http::HttpHeader( "Content-Length", nx::utils::to_string(d->response.messageBody.size()) ) );

    nx::String response = !multipartBoundary.isEmpty()
        ? d->response.toMultipartString(multipartBoundary)
        : d->response.toString();

    logRequestOrResponse("Sending response to", contentType, contentEncoding, response, d->response.messageBody);

    return response;
}

void QnTCPConnectionProcessor::sendResponse(
    int httpStatusCode,
    const nx::String& contentType,
    const nx::String& contentEncoding,
    const QByteArray& multipartBoundary,
    bool isUndefinedContentLength)
{
    sendBuffer(createResponse(
        httpStatusCode,
        contentType,
        contentEncoding,
        multipartBoundary,
        isUndefinedContentLength));
}

bool QnTCPConnectionProcessor::sendChunk( const nx::utils::ByteArray& chunk )
{
    return sendChunk( chunk.data(), chunk.size() );
}

bool QnTCPConnectionProcessor::sendChunk( const QByteArray& chunk )
{
    return sendChunk( chunk.data(), chunk.size() );
}

bool QnTCPConnectionProcessor::sendChunk(const nx::Buffer& chunk)
{
    return sendChunk(chunk.data(), chunk.size());
}

bool QnTCPConnectionProcessor::sendChunk( const char* data, int size )
{
    QByteArray result = QByteArray::number(size,16);
    result.append("\r\n");
    result.append(data, size);  //TODO/IMPL avoid copying by implementing writev in socket
    result.append("\r\n");

    return sendBuffer(result);
}

void QnTCPConnectionProcessor::pleaseStop()
{
    Q_D(QnTCPConnectionProcessor);
    {
        NX_MUTEX_LOCKER lock(&d->socketMutex);
        if (d->socket)
            d->socket->shutdown();
    }
    QnLongRunnable::pleaseStop();
}

nx::network::SocketAddress QnTCPConnectionProcessor::getForeignAddress() const
{
    Q_D(const QnTCPConnectionProcessor);
    NX_MUTEX_LOCKER lock(&d->socketMutex);
    return d->socket ? d->socket->getForeignAddress() : nx::network::SocketAddress();
}

QnTCPConnectionProcessor::ReadResult QnTCPConnectionProcessor::readRequest()
{
    Q_D(QnTCPConnectionProcessor);

    //QElapsedTimer globalTimeout;
    d->request = nx::network::http::Request();
    d->response = nx::network::http::Response();
    d->clientRequest.clear();
    d->requestBody.clear();
    d->requestLogged = false;

    qint64 maxTcpRequestSize = m_maxTcpRequestSize;
    std::optional<qint64> fullHttpMessageSize;
    while (!needToStop())
    {
        if (const auto bytes = d->socket->recv(d->tcpReadBuffer, TCP_READ_BUFFER_SIZE); bytes > 0)
        {
            //globalTimeout.restart();
            d->clientRequest.append((const char*) d->tcpReadBuffer, bytes);

            int messageSize = checkForBinaryProtocol(d->clientRequest);
            if (messageSize < 0)
               messageSize = isFullMessage(d->clientRequest, &fullHttpMessageSize);

            if (messageSize < 0)
                return ReadResult::incomplete;

            if (messageSize)
            {
                return ReadResult::ok;
            }
            else if (d->clientRequest.size() > maxTcpRequestSize)
            {
                if (maxTcpRequestSize < kLargeTcpRequestSize
                    && isLargeRequestAllowed(d->clientRequest))
                {
                    maxTcpRequestSize = kLargeTcpRequestSize;
                }
                else
                {
                    NX_WARNING(this, "Too large HTTP client request (%1 bytes, %2 allowed). Ignoring...",
                        d->clientRequest.size(), maxTcpRequestSize);
                    return ReadResult::error;
                }
            }
            if (fullHttpMessageSize)
                d->clientRequest.reserve(*fullHttpMessageSize);
        }
        else
        {
            const auto errorCode = SystemError::getLastOSErrorCode();
            if (bytes == 0 || nx::network::socketCannotRecoverFromError(errorCode))
                return ReadResult::error;
            return ReadResult::incomplete;
        }
    }

    return ReadResult::incomplete;
}

bool QnTCPConnectionProcessor::readSingleRequest()
{
    Q_D(QnTCPConnectionProcessor);

    d->request = nx::network::http::Request();
    d->response = nx::network::http::Response();
    d->requestBody.clear();
    d->currentRequestSize = 0;
    d->prevSocketError = SystemError::noError;

    // TODO: #akolesnikov It is more reliable to check for the first call of this method.
    if (!d->clientRequest.isEmpty())
    {
        // Due to a bug in QnTCPConnectionProcessor::readRequest(), d->clientRequest can contain
        // multiple interleaved requests. We have to parse them.
        NX_ASSERT (d->interleavedMessageData.isEmpty());
        d->interleavedMessageData = d->clientRequest; //< No copying here.
        d->clientRequest.clear();
        d->interleavedMessageDataPos = 0;
    }

    qint64 maxTcpRequestSize = m_maxTcpRequestSize;
    while (!needToStop() && d->socket->isConnected())
    {
        if (d->interleavedMessageDataPos == (size_t) d->interleavedMessageData.size())
        {
            // The buffer depleted, draining more data.
            const int bytesRead = d->socket->recv(d->tcpReadBuffer, TCP_READ_BUFFER_SIZE);
            if (bytesRead <= 0)
            {
                d->prevSocketError = SystemError::getLastOSErrorCode();
                NX_DEBUG(this, "Error reading request from %1: %2",
                    d->socket->getForeignAddress(), SystemError::toString( d->prevSocketError ));
                return false;
            }
            d->interleavedMessageData =
                QByteArray::fromRawData((const char*) d->tcpReadBuffer, bytesRead);
            d->interleavedMessageDataPos = 0;
        }

        size_t bytesParsed = 0;
        if (!d->httpStreamReader.parseBytes(
            nx::ConstBufferRefType(
                d->interleavedMessageData.data() + d->interleavedMessageDataPos,
                d->interleavedMessageData.size() - d->interleavedMessageDataPos),
            &bytesParsed) ||
            (d->httpStreamReader.state() ==
                nx::network::http::HttpStreamReader::ReadState::parseError))
        {
            //parse error
            return false;
        }
        d->currentRequestSize += bytesParsed;
        d->interleavedMessageDataPos += bytesParsed;

        if (d->currentRequestSize > maxTcpRequestSize)
        {
            if (maxTcpRequestSize < kLargeTcpRequestSize
                && d->httpStreamReader.message().type == nx::network::http::MessageType::request
                && isLargeRequestAllowed(d->httpStreamReader.message().request->requestLine.url))
            {
                maxTcpRequestSize = kLargeTcpRequestSize;
            }
            else
            {
                qWarning() << "Too large HTTP client request (" << d->currentRequestSize << " bytes"
                    ", " << maxTcpRequestSize << " allowed). Ignoring...";
                return false;
            }
        }

        if (d->httpStreamReader.state() ==
            nx::network::http::HttpStreamReader::ReadState::messageDone)
        {
            if (d->httpStreamReader.message().type != nx::network::http::MessageType::request)
                return false;
            // TODO: #akolesnikov We have parsed the message in d->httpStreamReader: should use it,
            // not copy.
            d->request = *d->httpStreamReader.message().request;
            d->protocol = d->request.requestLine.version.protocol;
            d->requestBody = d->httpStreamReader.fetchMessageBody().takeByteArray();

            if (d->owner)
                d->owner->applyModToRequest(&d->request);

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
    other.d_ptr->requestLogged = d->requestLogged;
}

nx::utils::Url QnTCPConnectionProcessor::getDecodedUrl() const
{
    Q_D(const QnTCPConnectionProcessor);
    return d->request.requestLine.url;
}

void QnTCPConnectionProcessor::execute(nx::Locker<nx::Mutex>& mutexLocker)
{
    m_needStop = false;
    mutexLocker.unlock();
    run();
    mutexLocker.relock();
}

nx::network::SocketAddress QnTCPConnectionProcessor::remoteHostAddress() const
{
    Q_D(const QnTCPConnectionProcessor);
    return d->socket ? d->socket->getForeignAddress() : nx::network::SocketAddress();
}

bool QnTCPConnectionProcessor::isConnectionSecure() const
{
    Q_D(const QnTCPConnectionProcessor);
    NX_ASSERT(d->socket);
    const auto socket = dynamic_cast<nx::network::AbstractEncryptedStreamSocket*>(d->socket.get());
    return socket && socket->isEncryptionEnabled();
}

std::unique_ptr<nx::network::AbstractStreamSocket> QnTCPConnectionProcessor::takeSocket()
{
    Q_D(QnTCPConnectionProcessor);
    NX_MUTEX_LOCKER lock(&d->socketMutex);
    nx::network::SocketGlobals::instance().allocationAnalyzer().recordObjectMove(d->socket.get());
    return std::move(d->socket);
}

int QnTCPConnectionProcessor::redirectTo(
    const nx::network::http::Method& method, const QByteArray& page, QByteArray* contentType)
{
    // Moved Permanently for get and head only because of incompatibility of many clients:
    //     https://developer.mozilla.org/en-US/docs/Web/HTTP/Status/301
    // Use Temporary Redirect instead of Permanent Redirect because permanent is experimental:
    //     https://datatracker.ietf.org/doc/html/rfc7238
    const auto code =
        (method == nx::network::http::Method::get || method == nx::network::http::Method::head)
            ? nx::network::http::StatusCode::movedPermanently
            : nx::network::http::StatusCode::temporaryRedirect;

    Q_D(QnTCPConnectionProcessor);
    std::tie(*contentType, d->response.messageBody) = generateErrorResponse(code);
    d->response.headers.insert(nx::network::http::HttpHeader("Location", page));
    return code;
}

int QnTCPConnectionProcessor::notFound(QByteArray& contentType)
{
    const auto code = nx::network::http::StatusCode::notFound;
    Q_D(QnTCPConnectionProcessor);
    std::tie(contentType, d->response.messageBody) = generateErrorResponse(code);
    return code;
}

bool QnTCPConnectionProcessor::isBinaryProtocol() const
{
    Q_D(const QnTCPConnectionProcessor);
    return d->binaryProtocol;
}

bool QnTCPConnectionProcessor::isConnectionCanBePersistent() const
{
    Q_D(const QnTCPConnectionProcessor);

    if (d->protocol.startsWith("RTSP"))
        return true;
    else if( d->request.requestLine.version == nx::network::http::http_1_1 )
    {
        return nx::utils::stricmp(
            nx::network::http::getHeaderValue(d->request.headers, "Connection"), "close") != 0;
    }
    else if (d->request.requestLine.version == nx::network::http::http_1_0)
    {
        return nx::utils::stricmp(
            nx::network::http::getHeaderValue(d->request.headers, "Connection"), "keep-alive") == 0;
    }
    else
    {
        return false;
    }
}

QnAuthSession QnTCPConnectionProcessor::authSession() const
{
    Q_D(const QnTCPConnectionProcessor);
    return authSession(d->accessRights);
}

QnAuthSession QnTCPConnectionProcessor::authSession(const Qn::UserAccessData& accessRights) const
{
    Q_D(const QnTCPConnectionProcessor);
    const auto& user = resourcePool()->getResourceById(accessRights.userId);
    return QnAuthSession(
        user ? user->getName() : QString(), d->request, d->socket->getForeignAddress().address);
}

void QnTCPConnectionProcessor::sendErrorResponse(
    nx::network::http::StatusCode::Value httpResult, const nx::String& errorDetails)
{
    Q_D(QnTCPConnectionProcessor);
    nx::String contentType;
    std::tie(contentType, d->response.messageBody) = generateErrorResponse(
        httpResult, errorDetails);
    sendResponse(httpResult, contentType);
}

void QnTCPConnectionProcessor::sendUnauthorizedResponse(
    nx::network::http::StatusCode::Value httpResult,
    const QByteArray& messageBody, const nx::String& details)
{
    Q_D(QnTCPConnectionProcessor);

    nx::String contentType = nx::network::http::header::ContentType::kHtml.toString();
    if( d->request.requestLine.method == nx::network::http::Method::get ||
        d->request.requestLine.method == nx::network::http::Method::head )
    {
        if (messageBody.isEmpty())
        {
            std::tie(contentType, d->response.messageBody) =
                generateErrorResponse(nx::network::http::StatusCode::unauthorized, details);
        }
        else
        {
            d->response.messageBody = messageBody;
        }
    }
    else if (messageBody.isEmpty())
    {
        if (nx::network::http::header::ContentType(nx::network::http::getHeaderValue(
                d->request.headers, nx::network::http::header::kAccept)).value
            == nx::network::http::header::ContentType::kJson.value)
        {
            d->response.messageBody =
                QJson::serialized(nx::network::rest::Result::unauthorized(details)).toStdString();
        }
    }

    if (nx::network::http::getHeaderValue(d->response.headers, Qn::SERVER_GUID_HEADER_NAME).empty())
    {
        d->response.headers.insert(nx::network::http::HttpHeader(
            Qn::SERVER_GUID_HEADER_NAME,
            peerId().toByteArray()));
    }

    auto acceptEncodingHeaderIter = d->request.headers.find( "Accept-Encoding" );
    QByteArray contentEncoding;
    if( acceptEncodingHeaderIter != d->request.headers.end() )
    {
        nx::network::http::header::AcceptEncodingHeader acceptEncodingHeader( acceptEncodingHeaderIter->second );
        if( acceptEncodingHeader.encodingIsAllowed( "identity" ) )
        {
            contentEncoding = "identity";
        }
        else if( acceptEncodingHeader.encodingIsAllowed( "gzip" ) )
        {
            contentEncoding = "gzip";
            if( !d->response.messageBody.empty() )
                d->response.messageBody = nx::utils::bstream::gzip::Compressor::compressData(d->response.messageBody);
        }
        else
        {
            //TODO #akolesnikov not supported encoding requested
        }
    }
    sendResponse(
        httpResult,
        d->response.messageBody.empty() ? QByteArray() : contentType,
        contentEncoding );
}

QnTcpListener* QnTCPConnectionProcessor::owner() const
{
    Q_D(const QnTCPConnectionProcessor);
    return d->owner;
}
