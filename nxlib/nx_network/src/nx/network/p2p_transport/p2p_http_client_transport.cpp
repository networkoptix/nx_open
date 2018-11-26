#include "p2p_http_client_transport.h"

#include <nx/utils/byte_stream/custom_output_stream.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/future.h>

namespace nx::network {

namespace {

static const int kMaxMessageQueueSize = 100;

} // namespace

P2PHttpClientTransport::P2PHttpClientTransport(
    HttpClientPtr readHttpClient,
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> onPostConnectionEstablished,
    websocket::FrameType messageType)
    :
    m_readHttpClient(std::move(readHttpClient)),
    m_messageType(messageType)
{
    m_readHttpClient->post(
        [this, onPostConnectionEstablished = std::move(onPostConnectionEstablished)]() mutable
        {
            startEstablishingPostConnection(std::move(onPostConnectionEstablished));
        });
}


P2PHttpClientTransport::~P2PHttpClientTransport()
{
    pleaseStopSync();
}

void P2PHttpClientTransport::readSomeAsync(nx::Buffer* const buffer, IoCompletionHandler handler)
{
    m_readHttpClient->post(
        [this, buffer, handler = std::move(handler)]() mutable
        {
            switch (m_state)
            {
                case State::failed:
                case State::gotGetConnectionOnly:
                    if (!m_incomingMessageQueue.empty())
                    {
                        NX_VERBOSE(this,
                            "The connection is in a failed state, but we have some"
                            " buffered messages left. Handing them out");
                        handoutIncomingMessage(buffer, std::move(handler));
                    }
                    else
                    {
                        NX_VERBOSE(this, lm("Connection failed. Reporting with a read handler"));
                        handler(SystemError::connectionAbort, 0);
                    }
                    break;
                default:
                    if (!m_incomingMessageQueue.empty())
                    {
                        handoutIncomingMessage(buffer, std::move(handler));
                    }
                    else
                    {
                        NX_VERBOSE(this,
                            "Got a read request but the incomingMessage queue is empty. "
                            "Saving it in the queue.");
                        if (m_userReadQueue.size() < kMaxMessageQueueSize)
                            m_userReadQueue.push(std::make_pair(buffer, std::move(handler)));
                        else
                            NX_WARNING(this, "Read queue overflow");
                    }
            }
        });
}

void P2PHttpClientTransport::handoutIncomingMessage(nx::Buffer* const buffer,
    IoCompletionHandler handler)
{
    NX_VERBOSE(this,
        "Got a ready message in incomingMessage queue. Handing it out");
    const auto queueBuffer = m_incomingMessageQueue.front();
    m_incomingMessageQueue.pop();
    buffer->append(queueBuffer);
    handler(SystemError::noError, queueBuffer.size());
}

void P2PHttpClientTransport::sendAsync(const nx::Buffer& buffer, IoCompletionHandler handler)
{
    m_readHttpClient->post(
        [this, buffer, handler = std::move(handler)]()
        {
            switch (m_state)
            {
            case State::failed:
            case State::gotGetConnectionOnly:
                return handler(SystemError::connectionAbort, 0);
            case State::postConnectionFailed:
                break; //< #TODO #akulikov
            case State::gotBothConnections:
                if (m_userWriteQueue.empty())
                {
                    auto url = m_readHttpClient->url();
                    url.setQuery(QUrlQuery());
                    doPost(url,
                        [this](bool success)
                        {

                        }, buffer);
                }
            };
        });
}

void P2PHttpClientTransport::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    m_readHttpClient->post(
        [this, aioThread]()
        {
            m_readHttpClient->bindToAioThread(aioThread);
            if (m_writeHttpClient)
                m_writeHttpClient->bindToAioThread(aioThread);
        });
}

void P2PHttpClientTransport::cancelIoInAioThread(nx::network::aio::EventType eventType)
{
    m_readHttpClient->cancelPostedCallsSync();
    if (m_writeHttpClient)
        m_writeHttpClient->cancelPostedCallsSync();
}

aio::AbstractAioThread* P2PHttpClientTransport::getAioThread() const
{
    return m_readHttpClient->getAioThread();
}

void P2PHttpClientTransport::pleaseStopSync()
{
    m_readHttpClient->pleaseStopSync();
    if (m_writeHttpClient)
        m_writeHttpClient->pleaseStopSync();
}

SocketAddress P2PHttpClientTransport::getForeignAddress() const
{
    nx::utils::promise<SocketAddress> p;
    auto f = p.get_future();
    m_readHttpClient->post(
        [this, p = std::move(p)]() mutable
        {
            p.set_value(m_readHttpClient->socket()->getForeignAddress());
        });
    return f.get();
}

void P2PHttpClientTransport::startEstablishingPostConnection(
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> onPostConnectionEstablished)
{
    m_writeHttpClient.reset(new http::AsyncClient());
    m_writeHttpClient->bindToAioThread(m_readHttpClient->getAioThread());
    auto url = m_readHttpClient->url();
    QUrlQuery urlQuery;
    urlQuery.addQueryItem("initial", "true");
    url.setQuery(urlQuery);
    doPost(url,
        [this, onPostConnectionEstablished = std::move(onPostConnectionEstablished)](bool success)
        {
            if (success)
            {
                m_state = State::gotBothConnections;
                NX_VERBOSE(this,
                    "The POST connection has been uccesfully established . Starting to read the "
                    "GET stream");
                startReading();
                onPostConnectionEstablished(SystemError::noError);
            }
            else
            {
                m_state = State::failed;
                NX_VERBOSE(this, "Failed to establish POST connection");
                onPostConnectionEstablished(SystemError::connectionAbort);
            }
        });
}

void P2PHttpClientTransport::doPost(const nx::utils::Url& url,
    utils::MoveOnlyFunc<void(bool)> doneHandler, const nx::Buffer& data)
{
    if (!data.isEmpty())
        m_writeHttpClient->setRequestBody(std::make_unique<PostBodySource>(m_messageType, data));
    else
        m_writeHttpClient->setRequestBody(nullptr);
    m_writeHttpClient->doPost(m_readHttpClient->url(),
        [this, doneHandler = std::move(doneHandler)]
        {
            const bool isResponseValid = m_writeHttpClient->response()
                && m_writeHttpClient->response()->statusLine.statusCode == http::StatusCode::ok;
            doneHandler(!m_writeHttpClient->failed() && isResponseValid);
        });
}

void P2PHttpClientTransport::startReading()
{
    auto url = m_readHttpClient->url();
    QUrlQuery urlQuery;
    urlQuery.addQueryItem("reading", "true");
    url.setQuery(urlQuery);
    m_readHttpClient->setOnResponseReceived(
        [this]()
        {
            m_multipartContentParser.setNextFilter(nx::utils::bstream::makeCustomOutputStream(
                [this](const QnByteArrayConstRef& data)
                {
                    if (m_incomingMessageQueue.size() < kMaxMessageQueueSize)
                    {
                        NX_VERBOSE(this, lm("Got incoming message %1").arg(data));
                        if (m_userReadQueue.empty())
                        {
                            m_incomingMessageQueue.push(data);
                        }
                        else
                        {
                            auto handlerPair = std::move(m_userReadQueue.front());
                            m_userReadQueue.pop();
                            handlerPair.first->append(data);
                            handlerPair.second(SystemError::noError, data.size());
                        }
                    }
                    else
                    {
                        NX_WARNING(this, lm("Incoming message queue overflow"));
                    }
                }));
            const auto& headers = m_readHttpClient->response()->headers;
            const auto contentTypeIt = headers.find("Content-Type");
            const bool isResponseMultiPart = contentTypeIt != headers.cend()
                && contentTypeIt->second.contains("multipart");
            NX_ASSERT(isResponseMultiPart);
            if (!isResponseMultiPart)
            {
                NX_WARNING(this, "Expected multipart response. It is not.");
                m_state = State::failed;
                return;
            }
        });
    m_readHttpClient->setOnSomeMessageBodyAvailable(
        [this]()
        {
            m_multipartContentParser.processData(m_readHttpClient->fetchMessageBodyBuffer());
        });
    m_readHttpClient->setOnDone(
        [this]()
        {
            NX_VERBOSE(this, "Read (GET) http client emitted 'onDone'. Moving to failed state.");
            m_state = State::failed;
        });
    m_readHttpClient->doGet(url);
}

P2PHttpClientTransport::PostBodySource::PostBodySource(websocket::FrameType messageType,
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
