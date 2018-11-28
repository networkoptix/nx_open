#include "p2p_http_server_transport.h"
#include <nx/network/http/http_async_client.h>
#include <nx/network/http/http_types.h>

namespace nx::network {

P2PHttpServerTransport::P2PHttpServerTransport(std::unique_ptr<AbstractStreamSocket> socket,
    websocket::FrameType messageType)
    :
    m_sendSocket(std::move(socket)),
    m_messageType(messageType)
{
    m_readContext.parser.setMessage(&m_readContext.message);
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
            m_readSocket->readSomeAsync(
                &m_readContext.buffer,
                [this, handler = std::move(handler), buffer](
                    SystemError::ErrorCode error,
                    size_t transferred) mutable
                {
                    onBytesRead(error, transferred, buffer, std::move(handler));
                });
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

    //size_t bytesProcessed;
    //switch( m_parser.parse(*buffer, &bytesProcessed))
    //{
    //case server::ParserState::done:
    //    *buffer = m_message.response->messageBody;
    //    m_message = http::Message();

    //}
}

void P2PHttpServerTransport::sendAsync(const nx::Buffer& buffer, IoCompletionHandler handler)
{
    m_sendSocket->post(
        [this, &buffer, handler = std::move(handler)]() mutable
        {
            nx::Buffer messageBuffer;
            if (m_firstSend)
            {
                http::Response initialResponse;
                auto& headers = initialResponse.headers;

                // #TODO: #akulikov Make some constants.
                headers.emplace("Host", m_sendSocket->getForeignHostName().toUtf8());
                headers.emplace("Content-Type", "multipart/mixed; boundary=ec2boundary");
                headers.emplace("Content-Length", "0");
                headers.emplace("Access-Control-Allow-Origin", "*");
                headers.emplace("Connection", "Keep-Alive");

                initialResponse.serialize(&messageBuffer);
                m_firstSend = false;
            }

            http::Response contentResponse;
            contentResponse.headers.emplace(
                "Content-Type",
                m_messageType == websocket::FrameType::text
                    ? "application/json" : "application/ubjson");
            contentResponse.messageBody = buffer;

            nx::Buffer contentBuffer;
            static const nx::Buffer boundary = "ec2boundary";
            contentResponse.serializeMultipartResponse(&contentBuffer, boundary);

            // #TODO: #akulikov Optimize this.
            messageBuffer += contentBuffer;
            m_sendSocket->sendAsync(
                messageBuffer,
                [this, handler = std::move(handler)](
                    SystemError::ErrorCode error,
                    size_t transferred)
                {
                    handler(error, transferred);
                });
        });
}

void P2PHttpServerTransport::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    m_sendSocket->post(
        [this, aioThread]()
        {
            m_sendSocket->bindToAioThread(aioThread);
            if (m_readSocket)
                m_readSocket->bindToAioThread(aioThread);
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

} // namespace nx::network
