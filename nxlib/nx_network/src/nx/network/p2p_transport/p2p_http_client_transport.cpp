#include "p2p_http_client_transport.h"

#include <nx/utils/byte_stream/custom_output_stream.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/future.h>

namespace nx::network {

namespace {

static const int kMaxMessageQueueSize = 500;

} // namespace

P2PHttpClientTransport::P2PHttpClientTransport(
    HttpClientPtr readHttpClient,
    const nx::Buffer& connectionGuid,
    websocket::FrameType messageType,
    const boost::optional<utils::Url>& url)
    :
    m_writeHttpClient(new http::AsyncClient),
    m_readHttpClient(std::move(readHttpClient)),
    m_messageType(messageType),
    m_url(url),
    m_connectionGuid(connectionGuid)
{
    using namespace std::chrono_literals;
    m_readHttpClient->setResponseReadTimeout(0ms);
    m_readHttpClient->setMessageBodyReadTimeout(0ms);
    m_readHttpClient->bindToAioThread(getAioThread());
    m_writeHttpClient->bindToAioThread(getAioThread());

    post([this]() { startReading(); });
}

void P2PHttpClientTransport::start(utils::MoveOnlyFunc<void(SystemError::ErrorCode)> onStart)
{
    if (onStart)
        post([onStart = std::move(onStart)] { onStart(SystemError::noError); });
}

P2PHttpClientTransport::~P2PHttpClientTransport()
{
    pleaseStopSync();
}

void P2PHttpClientTransport::stopWhileInAioThread()
{
    m_writeHttpClient.reset();
    m_readHttpClient.reset();
}

void P2PHttpClientTransport::readSomeAsync(nx::Buffer* const buffer, IoCompletionHandler handler)
{
    post(
        [this, buffer, handler = std::move(handler)]() mutable
        {
            // Don't call readSomeAsync() again, before a previous handler has called back.
            NX_ASSERT(!m_userReadHandlerPair);
            if (m_userReadHandlerPair)
                return handler(SystemError::notSupported, 0);

            if (!m_incomingMessageQueue.empty())
            {
                if (m_failed)
                {
                    NX_VERBOSE(
                        this,
                        "The connection is in a failed state, but we have some"
                        " buffered messages left. Handing them out");
                }

                const auto queueBuffer = m_incomingMessageQueue.front();
                m_incomingMessageQueue.pop();
                buffer->append(queueBuffer);
                handler(SystemError::noError, queueBuffer.size());
                return;
            }

            if (m_failed)
            {
                NX_VERBOSE(this, lm("The connection has failed. Reporting with a read handler"));
                handler(SystemError::connectionAbort, 0);
                return;
            }

            // No incoming message in the queue.
            m_userReadHandlerPair.reset(new std::pair<nx::Buffer* const, IoCompletionHandler>(
                buffer,
                std::move(handler)));
        });
}

void P2PHttpClientTransport::sendAsync(const nx::Buffer& buffer, IoCompletionHandler handler)
{
    post(
        [this, buffer, handler = std::move(handler)]() mutable
        {
            if (m_failed)
                return handler(SystemError::connectionAbort, 0);

            m_writeHttpClient->setRequestBody(std::make_unique<PostBodySource>(
                m_messageType,
                buffer));

            http::HttpHeaders additionalHeaders;
            additionalHeaders.emplace("X-NX-P2P-GUID", m_connectionGuid);
            m_writeHttpClient->setAdditionalHeaders(additionalHeaders);

            m_writeHttpClient->doPost(
                m_url ? *m_url : m_readHttpClient->url(),
                [this, handler = std::move(handler), bufferSize = buffer.size()]()
                {
                    const bool isResponseValid = m_writeHttpClient->response()
                        && m_writeHttpClient->response()->statusLine.statusCode
                            == http::StatusCode::ok;

                    const auto resultCode = !m_writeHttpClient->failed() && isResponseValid
                        ? SystemError::noError : SystemError::connectionAbort;
                    const std::size_t transferred = resultCode == SystemError::noError
                        ? bufferSize : 0;

                    handler(resultCode, transferred);
                });

        });
}

void P2PHttpClientTransport::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    BasicPollable::bindToAioThread(aioThread);
    m_readHttpClient->bindToAioThread(aioThread);
    if (m_writeHttpClient)
        m_writeHttpClient->bindToAioThread(aioThread);
}

void P2PHttpClientTransport::cancelIoInAioThread(nx::network::aio::EventType eventType)
{
    m_readHttpClient->socket()->cancelIOSync(eventType);
    if (m_writeHttpClient)
        m_writeHttpClient->socket()->cancelIOSync(eventType);
}

aio::AbstractAioThread* P2PHttpClientTransport::getAioThread() const
{
    return BasicPollable::getAioThread();
}

SocketAddress P2PHttpClientTransport::getForeignAddress() const
{
    nx::utils::promise<SocketAddress> p;
    auto f = p.get_future();
    m_readHttpClient->post(
        [this, &p]() mutable
        {
            p.set_value(m_readHttpClient->socket()->getForeignAddress());
        });
    return f.get();
}

void P2PHttpClientTransport::startReading()
{
    m_readHttpClient->setOnResponseReceived(
        [this]()
        {
            auto nextFilter = nx::utils::bstream::makeCustomOutputStream(
                [this](const QnByteArrayConstRef& data)
                {
                    if (m_incomingMessageQueue.size() < kMaxMessageQueueSize)
                    {
                        if (!m_userReadHandlerPair)
                        {
                            m_incomingMessageQueue.push(data);
                        }
                        else
                        {
                            m_userReadHandlerPair->first->append(data);

                            utils::ObjectDestructionFlag::Watcher watcher(&m_destructionFlag);
                            m_userReadHandlerPair->second(SystemError::noError, data.size());
                            if (watcher.objectDestroyed())
                                return;

                            m_userReadHandlerPair.reset();
                        }
                    }
                    else
                    {
                        NX_WARNING(this, lm("Incoming message queue overflow"));
                    }
                });

            m_multipartContentParser.setBoundary("ec2boundary");
            m_multipartContentParser.setNextFilter(nextFilter);

            const auto& headers = m_readHttpClient->response()->headers;
            const auto contentTypeIt = headers.find("Content-Type");
            const bool isResponseMultiPart = contentTypeIt != headers.cend()
                && contentTypeIt->second.contains("multipart");

            NX_ASSERT(isResponseMultiPart);
            if (!isResponseMultiPart)
            {
                NX_WARNING(this, "Expected a multipart response. It is not.");
                m_failed = true;
                return;
            }
        });

    m_readHttpClient->setOnSomeMessageBodyAvailable(
        [this]()
        {
            if (!m_multipartContentParser.processData(m_readHttpClient->fetchMessageBodyBuffer()))
                m_failed = true;
        });

     m_readHttpClient->setOnDone(
         [this]()
         {
             NX_VERBOSE(
                 this,
                 "The read (GET) http client emitted 'onDone'. Moving to a failed state.");
             m_failed = true;
         });

    //m_readHttpClient->doGet(m_url ? *m_url : m_readHttpClient->url());
}

P2PHttpClientTransport::PostBodySource::PostBodySource(
    websocket::FrameType messageType,
    const Buffer& data)
    :
    m_messageType(messageType),
    m_data(data)
{
}

http::StringType P2PHttpClientTransport::PostBodySource::mimeType() const
{
    if (m_messageType == websocket::FrameType::text)
        return "application/json";
    return "application/ubjson";
}

boost::optional<uint64_t> P2PHttpClientTransport::PostBodySource::contentLength() const
{
    return m_data.size();
}

void P2PHttpClientTransport::PostBodySource::readAsync(CompletionHandler completionHandler)
{
    completionHandler(SystemError::noError, m_data);
}

} // namespace nx::network
