#include "http_client.h"

#include <nx/utils/thread/mutex.h>
#include <nx/utils/std/future.h>
#include <nx/utils/log/log.h>

namespace {

const std::size_t kDefaultMaxInternalBufferSize = 200 * 1024 * 1024; //< 200MB should be enough

} // namespace

namespace nx_http {

HttpClient::HttpClient():
    m_done(false),
    m_error(false),
    m_terminated(false),
    m_maxInternalBufferSize(kDefaultMaxInternalBufferSize),
    m_expectOnlyBody(false)
{
    instantiateHttpClient();
}

HttpClient::~HttpClient()
{
    pleaseStop();
    m_asyncHttpClient->pleaseStopSync(false);
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
        static_cast<void(AsyncHttpClient::*)(const nx::utils::Url&)>(
            &nx_http::AsyncHttpClient::doGet), _1, url));
}

bool HttpClient::doUpgrade(
    const nx::utils::Url& url,
    const StringType& protocolToUpgradeTo)
{
    using namespace std::placeholders;
    return doRequest(std::bind(
        static_cast<void(AsyncHttpClient::*)(const nx::utils::Url&, const StringType&)>(
            &nx_http::AsyncHttpClient::doUpgrade), _1, url, protocolToUpgradeTo));
}

bool HttpClient::doPost(
    const nx::utils::Url& url,
    const nx_http::StringType& contentType,
    nx_http::StringType messageBody)
{
    using namespace std::placeholders;

    typedef void(AsyncHttpClient::*FuncType)(
        const nx::utils::Url& /*url*/,
        const nx_http::StringType& /*contentType*/,
        nx_http::StringType /*messageBody*/,
        bool /*includeContentLength*/);

    return doRequest(std::bind(
        static_cast<FuncType>(&nx_http::AsyncHttpClient::doPost),
        _1,
        url,
        contentType,
        std::move(messageBody),
        true));
}

bool HttpClient::doPut(
    const nx::utils::Url& url,
    const nx_http::StringType& contentType,
    nx_http::StringType messageBody)
{
    typedef void(AsyncHttpClient::*FuncType)(
        const nx::utils::Url& /*url*/,
        const nx_http::StringType& /*contentType*/,
        nx_http::StringType /*messageBody*/);

    return doRequest(std::bind(
        static_cast<FuncType>(&nx_http::AsyncHttpClient::doPut),
        std::placeholders::_1,
        url,
        contentType,
        std::move(messageBody)));
}

bool HttpClient::doDelete(const nx::utils::Url& url)
{
    using namespace std::placeholders;
    return doRequest(std::bind(
        static_cast<void(AsyncHttpClient::*)(const nx::utils::Url&)>(
            &nx_http::AsyncHttpClient::doDelete), _1, url));
}

const Response* HttpClient::response() const
{
    return m_asyncHttpClient->response();
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

    nx_http::BufferType result;
    if (m_error)
        return result;

    result.swap(m_msgBodyBuffer);
    return result;
}

void HttpClient::addAdditionalHeader(const StringType& key, const StringType& value)
{
    m_additionalHeaders.emplace_back(key, value);
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

void HttpClient::setSendTimeoutMs(unsigned int sendTimeoutMs)
{
    m_sendTimeoutMs = sendTimeoutMs;
}

void HttpClient::setResponseReadTimeoutMs(unsigned int responseReadTimeoutMs)
{
    m_responseReadTimeoutMs = responseReadTimeoutMs;
}

void HttpClient::setMessageBodyReadTimeoutMs(unsigned int messageBodyReadTimeoutMs)
{
    m_messageBodyReadTimeoutMs = messageBodyReadTimeoutMs;
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

void HttpClient::setProxyVia(const SocketAddress& proxyEndpoint)
{
    m_proxyEndpoint = proxyEndpoint;
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

            sock = std::move(m_asyncHttpClient->takeSocket());

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
    StringType* contentType)
{
    nx_http::HttpClient client;
    if (!client.doGet(url))
        return false;

    while (!client.eof())
        *msgBody += client.fetchMessageBodyBuffer();

    *contentType = getHeaderValue(client.response()->headers, "Content-Type");
    return true;
}

void HttpClient::instantiateHttpClient()
{
    m_asyncHttpClient = nx_http::AsyncHttpClient::create();
    connect(
        m_asyncHttpClient.get(), &AsyncHttpClient::responseReceived,
        this, &HttpClient::onResponseReceived,
        Qt::DirectConnection);
    connect(
        m_asyncHttpClient.get(), &AsyncHttpClient::someMessageBodyAvailable,
        this, &HttpClient::onSomeMessageBodyAvailable,
        Qt::DirectConnection);
    connect(
        m_asyncHttpClient.get(), &AsyncHttpClient::done,
        this, &HttpClient::onDone,
        Qt::DirectConnection);
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
            m_asyncHttpClient->setSubsequentReconnectTries(m_subsequentReconnectTries.get());
        if (m_reconnectTries)
            m_asyncHttpClient->setTotalReconnectTries(m_reconnectTries.get());
        if (m_sendTimeoutMs)
            m_asyncHttpClient->setSendTimeoutMs(m_sendTimeoutMs.get());
        if (m_responseReadTimeoutMs)
            m_asyncHttpClient->setResponseReadTimeoutMs(m_responseReadTimeoutMs.get());
        if (m_messageBodyReadTimeoutMs)
            m_asyncHttpClient->setMessageBodyReadTimeoutMs(m_messageBodyReadTimeoutMs.get());
        if (m_userAgent)
            m_asyncHttpClient->setUserAgent(m_userAgent.get());
        if (m_userName)
            m_asyncHttpClient->setUserName(m_userName.get());
        if (m_userPassword)
            m_asyncHttpClient->setUserPassword(m_userPassword.get());
        if (m_authType)
            m_asyncHttpClient->setAuthType(m_authType.get());
        if (m_proxyEndpoint)
            m_asyncHttpClient->setProxyVia(m_proxyEndpoint.get());

        m_asyncHttpClient->setExpectOnlyMessageBodyWithoutHeaders(m_expectOnlyBody);

        lk.relock();
    }

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
    // Message body buffer can be non-empty.
    m_msgBodyBuffer += m_asyncHttpClient->fetchMessageBodyBuffer();
    if ((std::size_t)m_msgBodyBuffer.size() > m_maxInternalBufferSize)
    {
        NX_LOG(
            lit("Sync HttpClient: internal buffer overflow. Max buffer size: %1, current buffer size: %2, requested url: %3.")
                .arg(m_maxInternalBufferSize).arg(m_msgBodyBuffer.size()).arg(url().toString()),
            cl_logWARNING);
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
        NX_LOG(
            lit("Sync HttpClient: internal buffer overflow. Max buffer size: %1, current buffer size: %2, requested url: %3.")
                .arg(m_maxInternalBufferSize).arg(m_msgBodyBuffer.size()).arg(url().toString()),
            cl_logWARNING);
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

void HttpClient::onReconnected()
{
    // TODO: #ak
}

} // namespace nx_http
