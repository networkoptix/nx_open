#include "p2p_http_server_transport.h"
#include <nx/network/http/http_async_client.h>
#include <nx/network/http/http_types.h>
#include <nx/utils/log/log.h>

namespace nx::p2p {

namespace {

const int kBufferSize = 4096;

static void resetBuffer(nx::Buffer* buffer)
{
    buffer->clear();
    buffer->reserve(kBufferSize);
}

} // namespace

P2PHttpServerTransport::P2PHttpServerTransport(
    std::unique_ptr<network::AbstractStreamSocket> socket,
    network::websocket::FrameType messageType)
    :
    m_sendSocket(std::move(socket)),
    m_messageType(messageType)
{
    m_readContext.parser.setMessage(&m_readContext.message);

    m_sendSocket->setNonBlockingMode(true);
    m_sendSocket->bindToAioThread(getAioThread());
    m_sendSocket->setRecvTimeout(0);
    m_timer.bindToAioThread(getAioThread());
    m_sendBuffer.reserve(kBufferSize);
    m_readContext.buffer.reserve(kBufferSize);
    m_sendChannelReadBuffer.reserve(kBufferSize);
}

void P2PHttpServerTransport::start(
    utils::MoveOnlyFunc<void(SystemError::ErrorCode)> onGetRequestReceived)
{
    m_onGetRequestReceived = std::move(onGetRequestReceived);

    m_timer.start(
        std::chrono::seconds(10),
        [this]()
        {
            if (m_onGetRequestReceived)
                m_onGetRequestReceived(SystemError::connectionAbort);
        });

    m_sendSocket->readSomeAsync(
        &m_sendBuffer,
        [this](SystemError::ErrorCode error, size_t transferred)
        {
            utils::ObjectDestructionFlag::Watcher watcher(&m_destructionFlag);
            m_onGetRequestReceived(
                error != SystemError::noError || transferred == 0
                    ? SystemError::connectionAbort
                    : SystemError::noError);

            if (watcher.objectDestroyed())
                return;

            m_onGetRequestReceived = nullptr;
            if (error != SystemError::noError)
            {
                NX_ASSERT(false, "Transport is supposed to be destroyed on error");
                return;
            }

            m_sendSocket->readSomeAsync(
                &m_sendChannelReadBuffer,
                [this](SystemError::ErrorCode error, size_t transferred)
                {
                    onReadFromSendSocket(error, transferred);
                });
        });
}

void P2PHttpServerTransport::onReadFromSendSocket(
    SystemError::ErrorCode error,
    size_t transferred)
{
    if (error != SystemError::noError || transferred == 0)
    {
        m_failed = true;
        if (m_userReadHandlerPair)
        {
            auto userReadHandlerPair = std::move(m_userReadHandlerPair);
            userReadHandlerPair->second(error, 0);
        }
    }
    else
    {
        resetBuffer(&m_sendChannelReadBuffer);
        m_sendSocket->readSomeAsync(
            &m_sendChannelReadBuffer,
            [this](SystemError::ErrorCode error, size_t transferred)
            {
                onReadFromSendSocket(error, transferred);
            });
    }
}

P2PHttpServerTransport::~P2PHttpServerTransport()
{
    pleaseStopSync();
}

void P2PHttpServerTransport::gotPostConnection(
    std::unique_ptr<network::AbstractStreamSocket> socket,
    const nx::Buffer& body)
{
    post(
        [this, socket = std::move(socket), body]() mutable
        {
            m_readSocket = std::move(socket);
            m_readSocket->setNonBlockingMode(true);
            m_readSocket->bindToAioThread(getAioThread());
            m_readSocket->setRecvTimeout(0);

            if (m_userReadHandlerPair)
            {
                auto userReadHandlerPair = std::move(m_userReadHandlerPair);
                if (body.isEmpty())
                {
                    readFromSocket(
                        userReadHandlerPair->first,
                        std::move(userReadHandlerPair->second));
                }
                else
                {
                    sendPostResponse(
                        SystemError::noError,
                        std::move(userReadHandlerPair->second),
                        [body, buffer = userReadHandlerPair->first](
                            SystemError::ErrorCode error,
                            network::IoCompletionHandler handler)
                        {
                            buffer->append(QByteArray::fromBase64(body));
                            handler(error, body.size());
                        });
                }
            }
            else if (!body.isEmpty())
            {
                m_providedPostBody = body;
            }
        });
}

void P2PHttpServerTransport::readSomeAsync(
    nx::Buffer* const buffer,
    network::IoCompletionHandler handler)
{
    post(
        [this, buffer, handler = std::move(handler)]() mutable
        {
            if (m_onGetRequestReceived || m_failed)
                return handler(SystemError::connectionAbort, 0);

            if (!m_providedPostBody.isEmpty())
            {
                sendPostResponse(
                    SystemError::noError,
                    std::move(handler),
                    [this, buffer](SystemError::ErrorCode error, network::IoCompletionHandler handler)
                    {
                        buffer->append(QByteArray::fromBase64(m_providedPostBody));
                        const auto bodySize = m_providedPostBody.size();
                        m_providedPostBody = nx::Buffer();
                        handler(error, bodySize);
                    });
            }
            else
            {
                readFromSocket(buffer, std::move(handler));
            }
        });
}

void P2PHttpServerTransport::readFromSocket(
    nx::Buffer* const buffer,
    network::IoCompletionHandler handler)
{
    if (!m_readSocket)
    {
        NX_ASSERT(!m_userReadHandlerPair);
        if (m_userReadHandlerPair)
        {
            m_userReadHandlerPair = nullptr;
            handler(SystemError::notSupported, 0);
        }

        m_userReadHandlerPair = UserReadHandlerPair(
            new std::pair<nx::Buffer* const, network::IoCompletionHandler>(
                buffer,
                std::move(handler)));

        return;
    }

    m_readSocket->readSomeAsync(
        &m_readContext.buffer,
        [this, handler = std::move(handler), buffer](
            SystemError::ErrorCode error,
            size_t transferred) mutable
        {
            onBytesRead(error, transferred, buffer, std::move(handler));
        });
}

void P2PHttpServerTransport::onBytesRead(
    SystemError::ErrorCode error,
    size_t transferred,
    nx::Buffer* const buffer,
    network::IoCompletionHandler handler)
{
    if (error != SystemError::noError)
    {
        handler(error, transferred);
        return;
    }

    if (transferred == 0)
    {
        NX_VERBOSE(this, "onBytesRead: Connection seems to have been closed by a remote peer.");
        m_readSocket.reset();

        NX_ASSERT(m_userReadHandlerPair == nullptr);
        if (m_userReadHandlerPair != nullptr)
        {
            handler(SystemError::notSupported, 0);
            return;
        }

        m_userReadHandlerPair = UserReadHandlerPair(
            new std::pair<nx::Buffer* const, network::IoCompletionHandler>(
                buffer,
                std::move(handler)));

        return;
    }

    auto completionHandler =
        [this, buffer](SystemError::ErrorCode error, network::IoCompletionHandler handler)
        {
            m_readContext.buffer = QByteArray::fromBase64(*buffer);
            *buffer = m_readContext.buffer;

            utils::ObjectDestructionFlag::Watcher watcher(&m_destructionFlag);
            handler(error, m_readContext.bytesParsed);
            if (watcher.objectDestroyed())
                return;

            m_readContext.reset();
        };

    size_t totalBytesProcessed = 0;

    while (totalBytesProcessed != transferred)
    {
        size_t bytesProcessed = 0;
        const auto parserState = m_readContext.parser.parse(m_readContext.buffer, &bytesProcessed);
        m_readContext.bytesParsed += bytesProcessed;
        totalBytesProcessed += bytesProcessed;

        switch(parserState)
        {
        case network::server::ParserState::done:
            buffer->append(m_readContext.parser.fetchMessageBody());
            sendPostResponse(SystemError::noError, std::move(handler), std::move(completionHandler));
            return;
        case network::server::ParserState::failed:
            sendPostResponse(SystemError::invalidData, std::move(handler), std::move(completionHandler));
            return;
        case network::server::ParserState::readingBody:
        case network::server::ParserState::readingMessage:
            buffer->append(m_readContext.parser.fetchMessageBody());
            m_readContext.buffer.remove(0, (int) bytesProcessed);
            break;
        case network::server::ParserState::init:
            NX_ASSERT(false, "Should never get here");
            sendPostResponse(SystemError::invalidData, std::move(handler), std::move(completionHandler));
            return;
        }
    }

    readFromSocket(buffer, std::move(handler));
}

void P2PHttpServerTransport::addDateHeader(network::http::HttpHeaders* headers)
{
    using namespace std::chrono;
    const auto dateTime = QDateTime::fromMSecsSinceEpoch(
        duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
    headers->emplace("Date", network::http::formatDateTime(dateTime));
}

void P2PHttpServerTransport::sendPostResponse(
    SystemError::ErrorCode error,
    network::IoCompletionHandler userHandler,
    utils::MoveOnlyFunc<void(SystemError::ErrorCode, network::IoCompletionHandler)> completionHandler)
{
    network::http::Response response;
    response.statusLine.statusCode = error == SystemError::noError
        ? network::http::StatusCode::ok
        : network::http::StatusCode::internalServerError;
    response.statusLine.version = network::http::http_1_1;
    response.statusLine.reasonPhrase = "Ok";

    response.headers.emplace("Content-Length", "0");
    response.headers.emplace("Connection", "keep-alive");
    addDateHeader(&response.headers);

    response.serialize(&m_responseBuffer);

    m_readSocket->sendAsync(
        m_responseBuffer,
        [this,
        readError = error,
        userHandler = std::move(userHandler),
        completionHandler = std::move(completionHandler)](
            SystemError::ErrorCode error,
            size_t /*transferred*/) mutable
        {
            m_responseBuffer.clear();
            if (readError != SystemError::noError)
                return completionHandler(readError, std::move(userHandler));

            if (error != SystemError::noError)
                return completionHandler(error, std::move(userHandler));

            completionHandler(SystemError::noError, std::move(userHandler));
        });
}

void P2PHttpServerTransport::sendAsync(
    const nx::Buffer& buffer,
    network::IoCompletionHandler handler)
{
    post(
        [this, &buffer, handler = std::move(handler)]() mutable
        {
            if (m_onGetRequestReceived || m_failed)
                return handler(SystemError::connectionAbort, 0);

            if (m_firstSend)
            {
                m_sendBuffer = makeInitialResponse();
                m_firstSend = false;
            }

            m_sendBuffer += buffer.toBase64() + "\r\n" + makeFrameHeader();
            m_sendSocket->sendAsync(
                m_sendBuffer,
                [this, handler = std::move(handler)](
                    SystemError::ErrorCode error,
                    size_t transferred)
                {
                    NX_VERBOSE(
                        this,
                        lm("Send completed. error: %1, transferred: %2").args(error, transferred));
                    resetBuffer(&m_sendBuffer);
                    handler(error, transferred);
                });
        });
}

QByteArray P2PHttpServerTransport::makeInitialResponse() const
{
    network::http::Response initialResponse;
    initialResponse.statusLine.statusCode = network::http::StatusCode::ok;
    initialResponse.statusLine.reasonPhrase = "Ok";
    initialResponse.statusLine.version = network::http::http_1_1;

    auto& headers = initialResponse.headers;
    headers.emplace("Host", m_sendSocket->getForeignHostName().toUtf8());
    headers.emplace("Content-Type", "multipart/mixed; boundary=ec2boundary");
    headers.emplace("Access-Control-Allow-Origin", "*");
    headers.emplace("Connection", "Keep-Alive");
    addDateHeader(&headers);

    nx::Buffer result;
    initialResponse.serialize(&result);
    result += makeFrameHeader();

    return result;
}

QByteArray P2PHttpServerTransport::makeFrameHeader() const
{
    nx::Buffer headerBuffer;

    network::http::serializeHeaders(
        {network::http::HttpHeader(
            "Content-Type",
            m_messageType == network::websocket::FrameType::text
                ? "application/json" : "application/ubjson")},
        &headerBuffer);

    return "--ec2boundary\r\n" + headerBuffer + "\r\n";
}

void P2PHttpServerTransport::bindToAioThread(network::aio::AbstractAioThread* aioThread)
{
    BasicPollable::bindToAioThread(aioThread);
    m_sendSocket->bindToAioThread(aioThread);
    if (m_readSocket)
        m_readSocket->bindToAioThread(aioThread);
    m_timer.bindToAioThread(aioThread);
}

void P2PHttpServerTransport::cancelIoInAioThread(nx::network::aio::EventType eventType)
{
    m_sendSocket->cancelIOSync(eventType);
    if (m_readSocket)
        m_readSocket->cancelIOSync(eventType);
}

network::aio::AbstractAioThread* P2PHttpServerTransport::getAioThread() const
{
    return BasicPollable::getAioThread();
}

network::SocketAddress P2PHttpServerTransport::getForeignAddress() const
{
    return m_sendSocket->getForeignAddress();
}

void P2PHttpServerTransport::stopWhileInAioThread()
{
    m_timer.cancelSync();
    m_sendSocket.reset();
    m_readSocket.reset();
}

void P2PHttpServerTransport::ReadContext::reset()
{
    message = network::http::Message();
    parser.reset();
    resetBuffer(&buffer);
    bytesParsed = 0;
}

} // namespace nx::network
