#include "p2p_http_server_transport.h"
#include <nx/network/http/http_async_client.h>
#include <nx/network/http/http_types.h>
#include <nx/utils/log/log.h>

namespace nx::network {

P2PHttpServerTransport::P2PHttpServerTransport(
    std::unique_ptr<AbstractStreamSocket> socket,
    websocket::FrameType messageType)
    :
    m_sendSocket(std::move(socket)),
    m_messageType(messageType)
{
    m_readContext.parser.setMessage(&m_readContext.message);
    m_timer.bindToAioThread(m_sendSocket->getAioThread());

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
            if (error != SystemError::noError || transferred == 0)
                return m_onGetRequestReceived(SystemError::connectionAbort);

            auto onGetRequestReceived = std::move(m_onGetRequestReceived);
            m_onGetRequestReceived = nullptr;
            onGetRequestReceived(SystemError::noError);
        });
}

P2PHttpServerTransport::~P2PHttpServerTransport()
{
    pleaseStopSync();
}

void P2PHttpServerTransport::gotPostConnection(std::unique_ptr<AbstractStreamSocket> socket)
{
    m_readSocket = std::move(socket);
    m_readSocket->bindToAioThread(m_sendSocket->getAioThread());
}

void P2PHttpServerTransport::readSomeAsync(nx::Buffer* const buffer, IoCompletionHandler handler)
{
    m_sendSocket->post(
        [this, buffer, handler = std::move(handler)]() mutable
        {
            if (m_onGetRequestReceived)
                return handler(SystemError::connectionAbort, 0);

            readFromSocket(buffer, std::move(handler));
        });
}

void P2PHttpServerTransport::readFromSocket(nx::Buffer* const buffer, IoCompletionHandler handler)
{
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
    IoCompletionHandler handler)
{
    if (error != SystemError::noError || transferred == 0)
    {
        handler(error, transferred);
        return;
    }

    size_t bytesProcessed;
    const auto parserState = m_readContext.parser.parse(m_readContext.buffer, &bytesProcessed);
    m_readContext.bytesParsed += bytesProcessed;

    switch(parserState)
    {
    case server::ParserState::done:
        *buffer = m_readContext.message.response->messageBody;
        handler(SystemError::noError, m_readContext.bytesParsed);
        m_readContext.reset();
        break;
    case server::ParserState::failed:
        handler(SystemError::invalidData, 0);
        m_readContext.reset();
        break;
    case server::ParserState::readingBody:
    case server::ParserState::readingMessage:
        readFromSocket(buffer, std::move(handler));
        break;
    case server::ParserState::init:
        NX_ASSERT(false, "Should never get here");
        handler(SystemError::invalidData, 0);
        m_readContext.reset();
        break;
    }
}

void P2PHttpServerTransport::sendAsync(const nx::Buffer& buffer, IoCompletionHandler handler)
{
    m_sendSocket->post(
        [this, &buffer, handler = std::move(handler)]() mutable
        {
            if (m_onGetRequestReceived)
                return handler(SystemError::connectionAbort, 0);

            if (m_firstSend)
            {
                m_sendBuffer = makeInitialResponse();
                m_firstSend = false;
            }

            const auto contentFrame = makeResponseFrame(buffer);
            // To make a peer actually receive and process payload frame
            const auto dummyFrame = makeResponseFrame("NX");
            m_sendBuffer += contentFrame + dummyFrame;

            NX_VERBOSE(this, lm("Sending %1 bytes: %2").args(m_sendBuffer.size(), m_sendBuffer));

            m_sendSocket->sendAsync(
                m_sendBuffer,
                [this, handler = std::move(handler)](
                    SystemError::ErrorCode error,
                    size_t transferred)
                {
                    NX_VERBOSE(
                        this,
                        lm("Send completed. error: %1, transferred: %2").args(error, transferred));
                    m_sendBuffer.clear();
                    handler(error, transferred);
                });
        });
}

QByteArray P2PHttpServerTransport::makeInitialResponse() const
{
    http::Response initialResponse;
    initialResponse.statusLine.statusCode = http::StatusCode::ok;
    initialResponse.statusLine.reasonPhrase = "Ok";
    initialResponse.statusLine.version = http::http_1_1;

    auto& headers = initialResponse.headers;
    headers.emplace("Host", m_sendSocket->getForeignHostName().toUtf8());
    headers.emplace("Content-Type", "multipart/mixed; boundary=ec2boundary");
    headers.emplace("Access-Control-Allow-Origin", "*");
    headers.emplace("Connection", "Keep-Alive");

    nx::Buffer result;
    initialResponse.serialize(&result);

    return result;
}

QByteArray P2PHttpServerTransport::makeResponseFrame(const nx::Buffer& payload) const
{
    http::Response contentResponse;
    contentResponse.headers.emplace(
        "Content-Type",
        m_messageType == websocket::FrameType::text ? "application/json" : "application/ubjson");
    contentResponse.messageBody = payload;

    nx::Buffer contentBuffer;
    static const nx::Buffer boundary = "--ec2boundary";
    contentResponse.serializeMultipartResponse(&contentBuffer, boundary);

    return contentBuffer;
}

void P2PHttpServerTransport::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    m_sendSocket->post(
        [this, aioThread]()
        {
            m_sendSocket->bindToAioThread(aioThread);
            if (m_readSocket)
                m_readSocket->bindToAioThread(aioThread);
            m_timer.bindToAioThread(aioThread);
        });
}

void P2PHttpServerTransport::cancelIoInAioThread(nx::network::aio::EventType eventType)
{
    m_sendSocket->cancelIOSync(eventType);
    if (m_readSocket)
        m_readSocket->cancelIOSync(eventType);
}

aio::AbstractAioThread* P2PHttpServerTransport::getAioThread() const
{
    return m_sendSocket->getAioThread();
}

void P2PHttpServerTransport::pleaseStopSync()
{
    m_sendSocket->pleaseStopSync();
    if (m_readSocket)
        m_readSocket->pleaseStopSync();
}

SocketAddress P2PHttpServerTransport::getForeignAddress() const
{
    return m_sendSocket->getForeignAddress();
}

void P2PHttpServerTransport::ReadContext::reset()
{
    message = http::Message();
    parser.reset();
    buffer.remove(0, bytesParsed);
    bytesParsed = 0;
}

} // namespace nx::network
