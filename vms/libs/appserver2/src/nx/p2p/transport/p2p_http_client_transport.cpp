#include "p2p_http_client_transport.h"

#include <nx/utils/byte_stream/custom_output_stream.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/future.h>
#include <nx/network/http/custom_headers.h>

namespace nx::p2p {

namespace {

static const int kMaxMessageQueueSize = 500;

} // namespace

P2PHttpClientTransport::P2PHttpClientTransport(
    HttpClientPtr readHttpClient,
    const nx::Buffer& connectionGuid,
    network::websocket::FrameType messageType,
    const boost::optional<utils::Url>& url)
    :
    m_writeHttpClient(new network::http::AsyncClient),
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
    m_writeHttpClient->setCredentials(m_readHttpClient->credentials());
}

void P2PHttpClientTransport::start(utils::MoveOnlyFunc<void(SystemError::ErrorCode)> onStart)
{
    post(
        [this, onStart = std::move(onStart)]() mutable
        {
            m_onStartHandler = std::move(onStart);
            startReading();
        });
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

void P2PHttpClientTransport::readSomeAsync(
    nx::Buffer* const buffer,
    network::IoCompletionHandler handler)
{
    post(
        [this, buffer, handler = std::move(handler)]() mutable
        {
            NX_ASSERT(
                !m_userReadHandlerPair,
                "Don't call readSomeAsync() again, before the previous handler has been invoked.");

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
                buffer->append(QByteArray::fromBase64(queueBuffer));
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
            m_userReadHandlerPair.reset(
                new std::pair<nx::Buffer* const, network::IoCompletionHandler>(
                    buffer,
                    std::move(handler)));
        });
}

void P2PHttpClientTransport::sendAsync(
    const nx::Buffer& buffer,
    network::IoCompletionHandler handler)
{
    post(
        [this, buffer, handler = std::move(handler)]() mutable
        {
            if (m_failed)
                return handler(SystemError::connectionAbort, 0);

            m_writeHttpClient->setRequestBody(std::make_unique<PostBodySource>(
                m_messageType,
                buffer.toBase64()));

            network::http::HttpHeaders additionalHeaders;
            additionalHeaders.emplace(Qn::EC2_CONNECTION_GUID_HEADER_NAME, m_connectionGuid);
            m_writeHttpClient->setAdditionalHeaders(additionalHeaders);

            m_writeHttpClient->doPost(
                m_url ? *m_url : m_readHttpClient->url(),
                [this, handler = std::move(handler), bufferSize = buffer.size()]()
                {
                    const bool isResponseValid = m_writeHttpClient->response()
                        && m_writeHttpClient->response()->statusLine.statusCode
                            == network::http::StatusCode::ok;

                    const auto resultCode = !m_writeHttpClient->failed() && isResponseValid
                        ? SystemError::noError : SystemError::connectionAbort;
                    const std::size_t transferred = resultCode == SystemError::noError
                        ? bufferSize : 0;

                    handler(resultCode, transferred);
                });

        });
}

void P2PHttpClientTransport::bindToAioThread(network::aio::AbstractAioThread* aioThread)
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

network::aio::AbstractAioThread* P2PHttpClientTransport::getAioThread() const
{
    return BasicPollable::getAioThread();
}

network::SocketAddress P2PHttpClientTransport::getForeignAddress() const
{
    return m_readHttpClient->executeInAioThreadSync(
        [this]() { return m_readHttpClient->socket()->getForeignAddress(); });
}

void P2PHttpClientTransport::startReading()
{
    m_readHttpClient->setOnResponseReceived(
        [this]()
        {
            if (m_onStartHandler)
            {
                utils::ObjectDestructionFlag::Watcher watcher(&m_destructionFlag);
                reportStartResult();
                if (watcher.objectDestroyed())
                    return;
            }

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
                            m_userReadHandlerPair->first->append(QByteArray::fromBase64(data));

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

            m_multipartContentParser.setNextFilter(nextFilter);

            const auto& headers = m_readHttpClient->response()->headers;
            const auto contentTypeIt = headers.find("Content-Type");

            NX_ASSERT(contentTypeIt != headers.end());
            if (contentTypeIt == headers.end() ||
                !m_multipartContentParser.setContentType(contentTypeIt->second))
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

             if (m_onStartHandler)
             {
                 utils::ObjectDestructionFlag::Watcher watcher(&m_destructionFlag);
                 reportStartResult();
                 if (watcher.objectDestroyed())
                     return;
             }

             m_failed = true;

             if (m_userReadHandlerPair)
             {
                 nx::Buffer outBuffer;
                 SystemError::ErrorCode errorCode = SystemError::noError;

                 if (!m_incomingMessageQueue.empty())
                 {
                     outBuffer = m_incomingMessageQueue.front();
                     m_incomingMessageQueue.pop();
                     m_userReadHandlerPair->first->append(QByteArray::fromBase64(outBuffer));

                 }
                 else
                 {
                     errorCode = SystemError::connectionAbort;
                 }

                 utils::ObjectDestructionFlag::Watcher watcher(&m_destructionFlag);
                 m_userReadHandlerPair->second(errorCode, outBuffer.size());
                 if (watcher.objectDestroyed())
                     return;

                 m_userReadHandlerPair.reset();
             }

         });

    m_readHttpClient->doGet(m_url ? *m_url : m_readHttpClient->url());
}

void P2PHttpClientTransport::reportStartResult()
{
    if (!m_onStartHandler)
        return;

    if (m_readHttpClient->failed())
        return nx::utils::swapAndCall(m_onStartHandler, m_readHttpClient->lastSysErrorCode());

    if (!nx::network::http::StatusCode::isSuccessCode(
            m_readHttpClient->response()->statusLine.statusCode))
    {
        return nx::utils::swapAndCall(m_onStartHandler, SystemError::connectionReset);
    }

    nx::utils::swapAndCall(m_onStartHandler, SystemError::noError);
}

P2PHttpClientTransport::PostBodySource::PostBodySource(
    network::websocket::FrameType messageType,
    const Buffer& data)
    :
    m_messageType(messageType),
    m_data(data)
{
}

network::http::StringType P2PHttpClientTransport::PostBodySource::mimeType() const
{
    if (m_messageType == network::websocket::FrameType::text)
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
