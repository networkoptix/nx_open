#include "http_client.h"

#include <nx/utils/log/log.h>
#include <nx/utils/std/future.h>
#include <nx/utils/type_utils.h>

#include "buffer_source.h"

namespace {

const std::size_t kDefaultMaxInternalBufferSize = 200 * 1024 * 1024; //< 200MB should be enough

} // namespace

namespace nx {
namespace network {
namespace http {

HttpClient::HttpClient():
    m_done(false),
    m_error(false),
    m_terminated(false),
    m_maxInternalBufferSize(kDefaultMaxInternalBufferSize)
{
}

HttpClient::HttpClient(std::unique_ptr<nx::network::AbstractStreamSocket> socket):
    HttpClient()
{
    m_socket = std::move(socket);
}

HttpClient::~HttpClient()
{
    pleaseStop();
    if (m_asyncHttpClient)
        m_asyncHttpClient->pleaseStopSync();
}

int HttpClient::totalRequestsSentViaCurrentConnection() const
{
    return m_asyncHttpClient ? m_asyncHttpClient->totalRequestsSentViaCurrentConnection() : 0;
}

int HttpClient::totalRequestsSent() const
{
    return m_asyncHttpClient ? m_asyncHttpClient->totalRequestsSent() : 0;
}

void HttpClient::pleaseStop()
{
    QnMutexLocker lk(&m_mutex);
    m_terminated = true;
    m_cond.wakeAll();
}

bool HttpClient::doGet(const nx::utils::Url& url)
{
    using namespace std::placeholders;
    return doRequest(std::bind(
        static_cast<void(AsyncClient::*)(const nx::utils::Url&)>(
            &nx::network::http::AsyncClient::doGet), _1, url));
}

bool HttpClient::doUpgrade(
    const nx::utils::Url& url,
    const StringType& protocolToUpgradeTo)
{
    using namespace std::placeholders;
    return doRequest(std::bind(
        static_cast<void(AsyncClient::*)(const nx::utils::Url&, const StringType&)>(
            &nx::network::http::AsyncClient::doUpgrade), _1, url, protocolToUpgradeTo));
}

bool HttpClient::doPost(
    const nx::utils::Url& url,
    const nx::network::http::StringType& contentType,
    nx::network::http::StringType messageBody)
{
    auto body = std::make_unique<BufferSource>(
        contentType, std::move(messageBody));

    return doRequest(
        [url, body = std::move(body)](
            nx::network::http::AsyncClient* client) mutable
        {
            client->doPost(
                url,
                nx::utils::static_unique_ptr_cast<AbstractMsgBodySource>(std::move(body)));
        });
}

bool HttpClient::doPut(
    const nx::utils::Url& url,
    const nx::network::http::StringType& contentType,
    nx::network::http::StringType messageBody)
{
    auto body = std::make_unique<BufferSource>(
        contentType, std::move(messageBody));

    return doRequest(
        [url, body = std::move(body)](
            nx::network::http::AsyncClient* client) mutable
        {
            client->doPut(
                url,
                nx::utils::static_unique_ptr_cast<AbstractMsgBodySource>(std::move(body)));
        });
}

bool HttpClient::doDelete(const nx::utils::Url& url)
{
    using namespace std::placeholders;
    return doRequest(std::bind(
        static_cast<void(AsyncClient::*)(const nx::utils::Url&)>(
            &nx::network::http::AsyncClient::doDelete), _1, url));
}

const Response* HttpClient::response() const
{
    QnMutexLocker lk(&m_mutex);
    return m_lastResponse ? &(*m_lastResponse) : nullptr;
}

SystemError::ErrorCode HttpClient::lastSysErrorCode() const
{
    return m_asyncHttpClient->lastSysErrorCode();
}

bool HttpClient::isValid() const
{
    QnMutexLocker lock(&m_mutex);
    return !m_error;
}

bool HttpClient::eof() const
{
    QnMutexLocker lk(&m_mutex);
    return (m_done && m_msgBodyBuffer.isEmpty()) || m_error;
}

BufferType HttpClient::fetchMessageBodyBuffer()
{
    QnMutexLocker lk(&m_mutex);

    while (!m_terminated && (m_msgBodyBuffer.isEmpty() && !m_done && !m_error))
        m_cond.wait(lk.mutex());

    nx::network::http::BufferType result;
    if (m_error)
        return result;

    result.swap(m_msgBodyBuffer);
    return result;
}

std::optional<BufferType> HttpClient::fetchEntireMessageBody()
{
    QByteArray buffer;
    while (!eof())
        buffer += fetchMessageBodyBuffer();

    if (m_error)
        return std::nullopt;

    return buffer;
}

void HttpClient::addAdditionalHeader(const StringType& key, const StringType& value)
{
    m_additionalHeaders.emplace_back(key, value);

    if (m_asyncHttpClient)
        m_asyncHttpClient->addAdditionalHeader(key, value);
}

void HttpClient::removeAdditionalHeader(const StringType& key)
{
    m_additionalHeaders.erase(
        std::remove_if(m_additionalHeaders.begin(), m_additionalHeaders.end(),
        [&key](const auto& header)
        {
            return header.first == key;
        }), m_additionalHeaders.end());

    if (m_asyncHttpClient)
        m_asyncHttpClient->removeAdditionalHeader(key);
}

const nx::utils::Url& HttpClient::url() const
{
    return m_asyncHttpClient->url();
}

const nx::utils::Url& HttpClient::contentLocationUrl() const
{
    return m_asyncHttpClient->contentLocationUrl();
}

StringType HttpClient::contentType() const
{
    return m_asyncHttpClient->contentType();
}

void HttpClient::setSubsequentReconnectTries(int reconnectTries)
{
    m_subsequentReconnectTries = reconnectTries;
}

void HttpClient::setTotalReconnectTries(int reconnectTries)
{
    m_reconnectTries = reconnectTries;
}

void HttpClient::setSendTimeout(
    std::chrono::milliseconds sendTimeout)
{
    m_sendTimeout = sendTimeout;
}

void HttpClient::setResponseReadTimeout(
    std::chrono::milliseconds responseReadTimeout)
{
    m_responseReadTimeout = responseReadTimeout;
}

void HttpClient::setMessageBodyReadTimeout(
    std::chrono::milliseconds messageBodyReadTimeout)
{
    m_messageBodyReadTimeout = messageBodyReadTimeout;
}

void HttpClient::setMaxNumberOfRedirects(int maxNumberOfRedirects)
{
    m_maxNumberOfRedirects = maxNumberOfRedirects;
}

void HttpClient::setUserAgent(const QString& userAgent)
{
    m_userAgent = userAgent;
}

void HttpClient::setUserName(const QString& userName)
{
    m_userName = userName;
    if (m_asyncHttpClient)
        m_asyncHttpClient->setUserName(userName);
}

void HttpClient::setUserPassword(const QString& userPassword)
{
    m_userPassword = userPassword;
    if (m_asyncHttpClient)
        m_asyncHttpClient->setUserPassword(userPassword);
}

void HttpClient::setAuthType(AuthType value)
{
    m_authType = value;
    if (m_asyncHttpClient)
        m_asyncHttpClient->setAuthType(value);
}

void HttpClient::setProxyVia(const SocketAddress& proxyEndpoint, bool isSecure)
{
    m_proxyEndpoint = proxyEndpoint;
    m_isProxySecure = isSecure;
}

void HttpClient::setDisablePrecalculatedAuthorization(bool value)
{
    m_precalculatedAuthorizationDisabled = value;
}

void HttpClient::setExpectOnlyMessageBodyWithoutHeaders(bool expectOnlyBody)
{
    m_expectOnlyBody = expectOnlyBody;
}

const std::unique_ptr<AbstractStreamSocket>& HttpClient::socket()
{
    return m_asyncHttpClient->socket();
}

std::unique_ptr<AbstractStreamSocket> HttpClient::takeSocket()
{
    // NOTE: m_asyncHttpClient->takeSocket() can only be called within m_asyncHttpClient's aio
    // thread.
    std::unique_ptr<AbstractStreamSocket> sock;
    nx::utils::promise<void> socketTakenPromise;
    m_asyncHttpClient->dispatch(
        [this, &sock, &socketTakenPromise]()
        {
            QnMutexLocker lock(&m_mutex);
            m_terminated = true;

            sock = m_asyncHttpClient->takeSocket();

            m_msgBodyBuffer.append(m_asyncHttpClient->fetchMessageBodyBuffer());
            socketTakenPromise.set_value();
        });
    socketTakenPromise.get_future().wait();

    if (!sock || !sock->setNonBlockingMode(false))
        return nullptr;
    return sock;
}

bool HttpClient::fetchResource(
    const nx::utils::Url& url,
    BufferType* msgBody,
    StringType* contentType,
    std::optional<std::chrono::milliseconds> customResponseReadTimeout)
{
    nx::network::http::HttpClient client;
    if (customResponseReadTimeout)
        client.setResponseReadTimeout(*customResponseReadTimeout);
    if (!client.doGet(url))
        return false;

    while (!client.eof())
        *msgBody += client.fetchMessageBodyBuffer();

    *contentType = getHeaderValue(client.response()->headers, "Content-Type");
    return true;
}

void HttpClient::instantiateHttpClient()
{
    m_asyncHttpClient = std::make_unique<nx::network::http::AsyncClient>(std::move(m_socket));

    m_asyncHttpClient->setOnResponseReceived(
        std::bind(&HttpClient::onResponseReceived, this));
    m_asyncHttpClient->setOnSomeMessageBodyAvailable(
        std::bind(&HttpClient::onSomeMessageBodyAvailable, this));
    m_asyncHttpClient->setOnDone(
        std::bind(&HttpClient::onDone, this));
}

template<typename AsyncClientFunc>
bool HttpClient::doRequest(AsyncClientFunc func)
{
    QnMutexLocker lk(&m_mutex);

    if (!m_done || m_error)
    {
        lk.unlock();

        // Have to re-establish connection if the previous message has not been read up to the end.
        if (m_asyncHttpClient)
        {
            m_asyncHttpClient->pleaseStopSync();
            m_asyncHttpClient.reset();
        }
        instantiateHttpClient();

        //setting up attributes
        for (const auto& keyValue : m_additionalHeaders)
            m_asyncHttpClient->addAdditionalHeader(keyValue.first, keyValue.second);
        if (m_subsequentReconnectTries)
            m_asyncHttpClient->setSubsequentReconnectTries(*m_subsequentReconnectTries);
        if (m_reconnectTries)
            m_asyncHttpClient->setTotalReconnectTries(*m_reconnectTries);
        if (m_sendTimeout)
            m_asyncHttpClient->setSendTimeout(*m_sendTimeout);
        if (m_responseReadTimeout)
            m_asyncHttpClient->setResponseReadTimeout(*m_responseReadTimeout);
        if (m_messageBodyReadTimeout)
            m_asyncHttpClient->setMessageBodyReadTimeout(*m_messageBodyReadTimeout);
        if (m_maxNumberOfRedirects)
            m_asyncHttpClient->setMaxNumberOfRedirects(*m_maxNumberOfRedirects);
        if (m_userAgent)
            m_asyncHttpClient->setUserAgent(*m_userAgent);
        if (m_userName)
            m_asyncHttpClient->setUserName(*m_userName);
        if (m_userPassword)
            m_asyncHttpClient->setUserPassword(*m_userPassword);
        if (m_authType)
            m_asyncHttpClient->setAuthType(*m_authType);
        if (m_proxyEndpoint)
            m_asyncHttpClient->setProxyVia(*m_proxyEndpoint, m_isProxySecure);

        m_asyncHttpClient->setDisablePrecalculatedAuthorization(m_precalculatedAuthorizationDisabled);
        m_asyncHttpClient->setExpectOnlyMessageBodyWithoutHeaders(m_expectOnlyBody);

        lk.relock();
    }

    m_lastResponse = std::nullopt;

    m_done = false;
    m_error = false;
    func(m_asyncHttpClient.get());

    m_msgBodyBuffer.clear();
    while (!m_terminated && (m_asyncHttpClient->state() < AsyncClient::State::sResponseReceived))
        m_cond.wait(lk.mutex());

    return m_asyncHttpClient->state() != AsyncClient::State::sFailed;
}

void HttpClient::onResponseReceived()
{
    QnMutexLocker lk(&m_mutex);

    m_lastResponse = *m_asyncHttpClient->response();

    // Message body buffer can be non-empty.
    m_msgBodyBuffer += m_asyncHttpClient->fetchMessageBodyBuffer();
    if ((std::size_t)m_msgBodyBuffer.size() > m_maxInternalBufferSize)
    {
        NX_WARNING(this,
            lm("Internal buffer overflow. "
                "Max buffer size: %1, current buffer size: %2, requested url: %3.")
                    .args(m_maxInternalBufferSize, m_msgBodyBuffer.size(), url()));
        m_done = true;
        m_error = true;
        m_asyncHttpClient->pleaseStopSync();
    }

    m_cond.wakeAll();
}

void HttpClient::onSomeMessageBodyAvailable()
{
    QnMutexLocker lk(&m_mutex);
    m_msgBodyBuffer += m_asyncHttpClient->fetchMessageBodyBuffer();
    if ((std::size_t)m_msgBodyBuffer.size() > m_maxInternalBufferSize)
    {
        NX_WARNING(this,
            lm("Internal buffer overflow. "
                "Max buffer size: %1, current buffer size: %2, requested url: %3.")
                    .args(m_maxInternalBufferSize, m_msgBodyBuffer.size(), url()));
        m_done = true;
        m_error = true;
        m_asyncHttpClient->pleaseStopSync();
    }
    m_cond.wakeAll();
}

void HttpClient::onDone()
{
    QnMutexLocker lk(&m_mutex);
    m_done = true;
    m_cond.wakeAll();
}

} // namespace nx
} // namespace network
} // namespace http
