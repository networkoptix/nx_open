// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "async_file_downloader.h"

#include <nx/network/url/url_builder.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>

namespace nx::network::http {

AsyncFileDownloader::AsyncFileDownloader(ssl::AdapterFunc adapterFunc): m_client(std::move(adapterFunc))
{
    m_state = State::init;
    m_client.setTimeouts(AsyncClient::Timeouts::defaults());
    m_client.setOnResponseReceived([this]() { onResponseReceived(); });
    m_client.setOnSomeMessageBodyAvailable([this]() { onSomeMessageBodyAvailable(); });
    m_client.setOnDone([this]() { onDone(); });
    if (m_client.getAioThread())
        bindToAioThread(m_client.getAioThread());
}

AsyncFileDownloader::~AsyncFileDownloader()
{
}

void AsyncFileDownloader::bindToAioThread(nx::network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);
    m_client.bindToAioThread(aioThread);
}

void AsyncFileDownloader::setOnResponseReceived(nx::utils::MoveOnlyFunc<void(std::optional<size_t>)> handler)
{
    m_onResponseReceived = std::move(handler);
}

void AsyncFileDownloader::setOnProgressHasBeenMade(nx::utils::MoveOnlyFunc<void(size_t, std::optional<double>)> handler)
{
    m_onProgressHasBeenMade = std::move(handler);
}

void AsyncFileDownloader::setOnDone(nx::utils::MoveOnlyFunc<void()> handler)
{
    m_onDone = std::move(handler);
}

void AsyncFileDownloader::setTimeouts(const AsyncClient::Timeouts& timeouts)
{
    m_client.setTimeouts(timeouts);
}

void AsyncFileDownloader::setAuthType(AuthType value)
{
    m_client.setAuthType(value);
}

const Credentials& AsyncFileDownloader::credentials() const
{
    return m_client.credentials();
}

void AsyncFileDownloader::setCredentials(const Credentials& credentials)
{
    m_client.setCredentials(credentials);
}

void AsyncFileDownloader::addAdditionalHeader(const std::string& key, const std::string& value)
{
    m_client.addAdditionalHeader(key, value);
}

void AsyncFileDownloader::setProxyCredentials(const Credentials& credentials)
{
    m_client.setProxyCredentials(credentials);
}

void AsyncFileDownloader::setProxyVia(
    const SocketAddress& proxyEndpoint, bool isSecure, ssl::AdapterFunc adapterFunc)
{
    m_client.setProxyVia(proxyEndpoint, isSecure, std::move(adapterFunc));
}

void AsyncFileDownloader::start(const nx::utils::Url& url, std::shared_ptr<QFile> file)
{
    if (!NX_ASSERT(m_state == State::init || m_state == State::stopped))
        return;

    if (!NX_ASSERT(file->isWritable()))
    {
        m_state = State::failedIo;
        return;
    }

    m_state = State::requesting;
    m_file = file;
    m_contentLength.reset();

    m_client.doGet(url);
}

void AsyncFileDownloader::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();
    m_client.pleaseStopSync();

    m_state = State::stopped;
    m_file.reset();
}

std::string AsyncFileDownloader::contentType() const
{
    return m_client.contentType();
}

std::optional<size_t> AsyncFileDownloader::contentLength() const
{
    return m_contentLength;
}

const Response* AsyncFileDownloader::response() const
{
    return m_client.response();
}

bool AsyncFileDownloader::hasRequestSucceeded() const
{
    return m_client.hasRequestSucceeded();
}

bool AsyncFileDownloader::failed() const
{
    return (m_state == State::failedIo) || m_client.failed();
}

SystemError::ErrorCode AsyncFileDownloader::lastSysErrorCode() const
{
    if (m_state == State::failedIo)
        return SystemError::ioError;

    return m_client.lastSysErrorCode();
}

void AsyncFileDownloader::onResponseReceived()
{
    if (!NX_ASSERT(m_state == State::requesting))
        return;

    m_state = State::responseReceived;

    auto it = m_client.response()->headers.find("Content-Length");
    if (it != m_client.response()->headers.end())
        m_contentLength = QString::fromStdString(it->second).toULong();

    if (m_onResponseReceived)
        m_onResponseReceived(m_contentLength);
}

void AsyncFileDownloader::onSomeMessageBodyAvailable()
{
    if (!NX_ASSERT(m_state == State::responseReceived || m_state == State::downloading))
        return;

    m_state = State::downloading;

    auto buf = m_client.fetchMessageBodyBuffer();

    if (m_file->write(buf.toByteArray()) < 0)
    {
        m_state = State::failedIo;
        m_file.reset();
        m_client.pleaseStopSync();

        if (m_onDone)
            m_onDone();

        return;
    }

    if (m_onProgressHasBeenMade)
    {
        std::optional<double> percentage;
        if (m_contentLength && *m_contentLength != 0)
            percentage = (double)m_file->size() / *m_contentLength;

        m_onProgressHasBeenMade(buf.size(), percentage);
    }
}

void AsyncFileDownloader::onDone()
{
    if (!NX_ASSERT(m_state == State::requesting || m_state == State::responseReceived || m_state == State::downloading))
        return;

    m_state = State::done;
    m_file.reset();

    if (m_onDone)
        m_onDone();
}

} // namespace nx::network::http
