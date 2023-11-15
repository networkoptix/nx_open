// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "http_client.h"

#include <nx/utils/elapsed_timer.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/future.h>
#include <nx/utils/type_utils.h>

#include "buffer_source.h"

namespace nx::network::http {

HttpClient::HttpClient(ssl::AdapterFunc adapterFunc): m_adapterFunc(std::move(adapterFunc))
{
}

HttpClient::HttpClient(std::unique_ptr<AbstractStreamSocket> socket, ssl::AdapterFunc adapterFunc):
    m_socket(std::move(socket)), m_adapterFunc(std::move(adapterFunc))
{
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
    NX_MUTEX_LOCKER lk(&m_mutex);
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
    const std::string& protocolToUpgradeTo)
{
    return doRequest(
        [url, protocolToUpgradeTo](AsyncClient* client)
        {
            client->doUpgrade(url, protocolToUpgradeTo);
        });
}

bool HttpClient::doUpgrade(
    const nx::utils::Url& url,
    Method method,
    const std::string& protocolToUpgradeTo)
{
    return doRequest(
        [url, method, protocolToUpgradeTo](AsyncClient* client)
        {
            client->doUpgrade(url, method, protocolToUpgradeTo);
        });
}

bool HttpClient::doConnect(const nx::utils::Url& proxyUrl, const std::string& targetHost)
{
    return doRequest(
        [&proxyUrl, &targetHost](auto client) { client->doConnect(proxyUrl, targetHost); });
}

bool HttpClient::doPost(
    const nx::utils::Url& url,
    std::unique_ptr<AbstractMsgBodySource> body)
{
    return doRequest(
        [url, body = std::move(body)](
            nx::network::http::AsyncClient* client) mutable
        {
            client->doPost(url, std::move(body));
        });
}

bool HttpClient::doPost(
    const nx::utils::Url& url,
    const std::string& contentType,
    nx::Buffer messageBody)
{
    return doPost(
        url,
        std::make_unique<BufferSource>(contentType, std::move(messageBody)));
}

bool HttpClient::doPost(const nx::utils::Url& url)
{
    return doRequest([url](nx::network::http::AsyncClient* client) { client->doPost(url); });
}

bool HttpClient::doPut(
    const nx::utils::Url& url,
    const std::string& contentType,
    nx::Buffer messageBody)
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

bool HttpClient::doPatch(
    const nx::utils::Url& url,
    const std::string& contentType,
    nx::Buffer messageBody)
{
    auto body = std::make_unique<BufferSource>(
        contentType, std::move(messageBody));

    return doRequest(
        [url, body = std::move(body)](
            nx::network::http::AsyncClient* client) mutable
        {
            client->doPatch(
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

bool HttpClient::doRequest(const Method& method, const nx::utils::Url& url)
{
    using namespace std::placeholders;
    return doRequest(std::bind(
        static_cast<void(AsyncClient::*)(const Method&, const nx::utils::Url&)>(
            &nx::network::http::AsyncClient::doRequest), _1, method, url));
}

const Request& HttpClient::request() const
{
    return m_asyncHttpClient->request();
}

const Response* HttpClient::response() const
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    return m_lastResponse ? &(*m_lastResponse) : nullptr;
}

SystemError::ErrorCode HttpClient::lastSysErrorCode() const
{
    return m_asyncHttpClient->lastSysErrorCode();
}

bool HttpClient::isValid() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return !m_error && !m_terminated;
}

bool HttpClient::eof() const
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    return (m_done && m_msgBodyBuffer.empty()) || m_error || m_terminated;
}

bool HttpClient::hasRequestSucceeded() const
{
    return m_asyncHttpClient->hasRequestSucceeded();
}

nx::Buffer HttpClient::fetchMessageBodyBuffer()
{
    NX_MUTEX_LOCKER lk(&m_mutex);

    while (!m_terminated && (m_msgBodyBuffer.empty() && !m_done && !m_error))
        m_cond.wait(lk.mutex());

    nx::Buffer result;
    if (m_error)
        return result;

    result.swap(m_msgBodyBuffer);
    return result;
}

std::optional<nx::Buffer> HttpClient::fetchEntireMessageBody(
    std::optional<std::chrono::milliseconds> timeout)
{
    nx::Buffer buffer;
    nx::utils::ElapsedTimer timer;
    const bool hasTimeout = timeout && timeout->count() > 0;
    if (hasTimeout)
        timer.restart();
    while (!eof())
    {
        buffer += fetchMessageBodyBuffer();
        if (hasTimeout && timer.hasExpired(*timeout))
        {
            m_error = true;
            break;
        }
    }

    if (m_error)
        return std::nullopt;

    return buffer;
}

void HttpClient::addAdditionalHeader(const std::string& key, const std::string& value)
{
    m_additionalHeaders.emplace_back(key, value);
}

void HttpClient::removeAdditionalHeader(const std::string& key)
{
    m_additionalHeaders.erase(
        std::remove_if(
            m_additionalHeaders.begin(), m_additionalHeaders.end(),
            [&key](const auto& header)
            {
                return header.first == key;
            }),
        m_additionalHeaders.end());
}

const nx::utils::Url& HttpClient::url() const
{
    return m_asyncHttpClient->url();
}

const nx::utils::Url& HttpClient::contentLocationUrl() const
{
    return m_asyncHttpClient->contentLocationUrl();
}

std::string HttpClient::contentType() const
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
    m_timeouts.sendTimeout = sendTimeout;
}

void HttpClient::setResponseReadTimeout(
    std::chrono::milliseconds responseReadTimeout)
{
    m_timeouts.responseReadTimeout = responseReadTimeout;
}

void HttpClient::setMessageBodyReadTimeout(
    std::chrono::milliseconds messageBodyReadTimeout)
{
    m_timeouts.messageBodyReadTimeout = messageBodyReadTimeout;
}

void HttpClient::setTimeouts(const AsyncClient::Timeouts& timeouts)
{
    m_timeouts = timeouts;
}

void HttpClient::setAllTimeouts(std::chrono::milliseconds timeout)
{
    setTimeouts({ timeout, timeout, timeout });
}

AsyncClient::Timeouts HttpClient::timeouts() const
{
    return m_timeouts;
}

void HttpClient::setMaxNumberOfRedirects(int maxNumberOfRedirects)
{
    m_maxNumberOfRedirects = maxNumberOfRedirects;
}

void HttpClient::setUserAgent(const std::string& userAgent)
{
    m_userAgent = userAgent;
}

void HttpClient::setAuthType(AuthType value)
{
    m_authType = value;
    if (m_asyncHttpClient)
        m_asyncHttpClient->setAuthType(value);
}

void HttpClient::setCredentials(const Credentials& credentials)
{
    m_credentials = credentials;
    if (m_asyncHttpClient)
        m_asyncHttpClient->setCredentials(credentials);
}

std::optional<Credentials> HttpClient::credentials() const
{
    return m_credentials;
}

void HttpClient::setProxyCredentials(const Credentials& credentials)
{
    m_proxyCredentials = credentials;
    if (m_asyncHttpClient)
        m_asyncHttpClient->setProxyCredentials(credentials);
}

void HttpClient::setProxyVia(
    const SocketAddress& proxyEndpoint, bool isSecure, ssl::AdapterFunc adapterFunc)
{
    m_proxyEndpoint = proxyEndpoint;
    m_isProxySecure = isSecure;
    m_proxyAdapterFunc = std::move(adapterFunc);
    if (m_asyncHttpClient)
        m_asyncHttpClient->setProxyVia(*m_proxyEndpoint, m_isProxySecure, m_proxyAdapterFunc);
}

void HttpClient::setDisablePrecalculatedAuthorization(bool value)
{
    m_precalculatedAuthorizationDisabled = value;
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
            NX_MUTEX_LOCKER lock(&m_mutex);
            m_terminated = true;

            sock = m_asyncHttpClient->takeSocket();

            m_msgBodyBuffer.append(m_asyncHttpClient->fetchMessageBodyBuffer());
            socketTakenPromise.set_value();
        });
    socketTakenPromise.get_future().wait();

    if (!sock || !sock->setNonBlockingMode(false))
        return nullptr;

    SocketGlobals::instance().allocationAnalyzer().recordObjectMove(sock.get());
    return sock;
}

bool HttpClient::fetchResource(
    const nx::utils::Url& url,
    nx::Buffer* msgBody,
    std::string* contentType,
    std::optional<std::chrono::milliseconds> customResponseReadTimeout,
    ssl::AdapterFunc adapterFunc)
{
    nx::network::http::HttpClient client(std::move(adapterFunc));
    if (customResponseReadTimeout)
        client.setResponseReadTimeout(*customResponseReadTimeout);
    if (!client.doGet(url))
        return false;

    while (!client.eof())
        *msgBody += client.fetchMessageBodyBuffer();

    *contentType = getHeaderValue(client.response()->headers, "Content-Type");
    return true;
}

void HttpClient::instantiateAsyncClient()
{
    m_asyncHttpClient = std::make_unique<AsyncClient>(std::move(m_socket), m_adapterFunc);
    m_asyncHttpClient->setOnResponseReceived(
        std::bind(&HttpClient::onResponseReceived, this));
    m_asyncHttpClient->setOnSomeMessageBodyAvailable(
        std::bind(&HttpClient::onSomeMessageBodyAvailable, this));

    // NOTE: Doing post before invoking onDone to let m_asyncHttpClient complete its processing.
    m_asyncHttpClient->setOnDone(
        [this]() { m_asyncHttpClient->post([this]() { onDone(); }); });
}

void HttpClient::configureAsyncClient()
{
    m_asyncHttpClient->setAdditionalHeaders({});
    for (const auto& keyValue: m_additionalHeaders)
        m_asyncHttpClient->addAdditionalHeader(keyValue.first, keyValue.second);

    if (m_subsequentReconnectTries)
        m_asyncHttpClient->setSubsequentReconnectTries(*m_subsequentReconnectTries);
    if (m_reconnectTries)
        m_asyncHttpClient->setTotalReconnectTries(*m_reconnectTries);
    m_asyncHttpClient->setTimeouts(m_timeouts);
    if (m_maxNumberOfRedirects)
        m_asyncHttpClient->setMaxNumberOfRedirects(*m_maxNumberOfRedirects);
    if (m_userAgent)
        m_asyncHttpClient->setUserAgent(*m_userAgent);
    if (m_authType)
        m_asyncHttpClient->setAuthType(*m_authType);
    if (m_credentials)
        m_asyncHttpClient->setCredentials(*m_credentials);
    if (m_proxyCredentials)
        m_asyncHttpClient->setProxyCredentials(*m_proxyCredentials);
    if (m_proxyEndpoint)
        m_asyncHttpClient->setProxyVia(*m_proxyEndpoint, m_isProxySecure, m_proxyAdapterFunc);

    m_asyncHttpClient->setDisablePrecalculatedAuthorization(m_precalculatedAuthorizationDisabled);
}

template<typename AsyncClientFunc>
bool HttpClient::doRequest(AsyncClientFunc func)
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    if (m_terminated)
        return false;

    if (!m_done || m_error)
    {
        lk.unlock();

        // Have to re-establish connection if the previous message has not been read up to the end.
        if (m_asyncHttpClient)
        {
            m_asyncHttpClient->pleaseStopSync();
            m_asyncHttpClient.reset();
        }
        instantiateAsyncClient();

        lk.relock();
    }

    configureAsyncClient();

    m_lastResponse = std::nullopt;

    m_done = false;
    m_error = false;
    func(m_asyncHttpClient.get());

    m_msgBodyBuffer.clear();
    while (!m_terminated && !m_lastResponse && m_msgBodyBuffer.empty() && !m_done)
        m_cond.wait(lk.mutex());

    return m_lastResponse != std::nullopt;
}

void HttpClient::onResponseReceived()
{
    NX_MUTEX_LOCKER lk(&m_mutex);

    m_lastResponse = *m_asyncHttpClient->response();

    // Message body buffer can be non-empty.
    m_msgBodyBuffer += m_asyncHttpClient->fetchMessageBodyBuffer();
    if ((std::size_t)m_msgBodyBuffer.size() > m_maxInternalBufferSize)
    {
        NX_WARNING(this,
            nx::format("Internal buffer overflow. "
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
    NX_MUTEX_LOCKER lk(&m_mutex);
    m_msgBodyBuffer += m_asyncHttpClient->fetchMessageBodyBuffer();
    if ((std::size_t)m_msgBodyBuffer.size() > m_maxInternalBufferSize)
    {
        NX_WARNING(this,
            nx::format("Internal buffer overflow. "
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
    NX_MUTEX_LOCKER lk(&m_mutex);
    m_done = true;
    m_error = m_asyncHttpClient->failed();
    m_cond.wakeAll();
}

ssl::AdapterFunc HttpClient::setAdapterFunc(ssl::AdapterFunc adapterFunc)
{
    auto result = std::exchange(m_adapterFunc, std::move(adapterFunc));
    if (m_asyncHttpClient && m_adapterFunc)
        return m_asyncHttpClient->setAdapterFunc(m_adapterFunc);
    return result;
}

nx::network::http::StatusCode::Value HttpClient::statusCode() const
{
    if (auto resp = response())
        return (nx::network::http::StatusCode::Value) resp->statusLine.statusCode;
    return nx::network::http::StatusCode::undefined;
}

} // namespace nx::network::http
