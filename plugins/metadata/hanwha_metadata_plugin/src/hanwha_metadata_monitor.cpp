#include "hanwha_metadata_monitor.h"
#include "hanwha_bytestream_filter.h"

#include <chrono>
#include <iostream>

#include <nx/utils/std/cpp14.h>

namespace nx {
namespace mediaserver {
namespace plugins {

namespace {
    const QString kMonitorUrlTemplate(
        "http://%1:%2/stw-cgi/eventstatus.cgi?msubmenu=eventstatus&action=monitordiff");

    const int kDefaultHttpPort = 80;

    const std::chrono::minutes kKeepAliveTimeout(7);
} // namespace


HanwhaMetadataMonitor::HanwhaMetadataMonitor(
    const Hanwha::DriverManifest& manifest,
    const nx::utils::Url& url,
    const QAuthenticator& auth)
    :
    m_manifest(manifest),
    m_url(buildMonitoringUrl(url)),
    m_auth(auth)
{

}

HanwhaMetadataMonitor::~HanwhaMetadataMonitor()
{
    stopMonitoring();
}

void HanwhaMetadataMonitor::startMonitoring()
{
    QnMutexLocker lock(&m_mutex);
    if (m_started)
        return;

    m_started = true;

    initMonitorUnsafe();
}

void HanwhaMetadataMonitor::stopMonitoring()
{
    nx_http::AsyncHttpClientPtr httpClient;
    {
        QnMutexLocker lock(&m_mutex);
        m_started = false;

        httpClient.swap(m_httpClient);
    }

    if (httpClient)
        httpClient->pleaseStopSync(false);
}

void HanwhaMetadataMonitor::addHandler(const QString& handlerId, const Handler& handler)
{
    QnMutexLocker lock(&m_mutex);
    m_handlers[handlerId] = handler;
}

void HanwhaMetadataMonitor::removeHandler(const QString& handlerId)
{
    QnMutexLocker lock(&m_mutex);
    m_handlers.remove(handlerId);
}

void HanwhaMetadataMonitor::clearHandlers()
{
    QnMutexLocker lock(&m_mutex);
    m_handlers.clear();
}

utils::Url HanwhaMetadataMonitor::buildMonitoringUrl(const utils::Url &url) const
{
    return nx::utils::Url(kMonitorUrlTemplate
        .arg(url.host())
        .arg(url.port(kDefaultHttpPort)));
}

void HanwhaMetadataMonitor::initMonitorUnsafe()
{
    auto httpClient = nx_http::AsyncHttpClient::create();

    connect(
        httpClient.get(), &nx_http::AsyncHttpClient::responseReceived,
        this, &HanwhaMetadataMonitor::at_responseReceived,
        Qt::DirectConnection);

    connect(
        httpClient.get(), &nx_http::AsyncHttpClient::someMessageBodyAvailable,
        this, &HanwhaMetadataMonitor::at_someBytesAvailable,
        Qt::DirectConnection);

    connect(
        httpClient.get(), &nx_http::AsyncHttpClient::done,
        this, &HanwhaMetadataMonitor::at_connectionClosed,
        Qt::DirectConnection);

    httpClient->setTotalReconnectTries(nx_http::AsyncHttpClient::UNLIMITED_RECONNECT_TRIES);
    httpClient->setUserName(m_auth.user());
    httpClient->setUserPassword(m_auth.password());
    httpClient->setMessageBodyReadTimeoutMs(
        std::chrono::duration_cast<std::chrono::milliseconds>(kKeepAliveTimeout).count());

    auto handler =
        [this](const HanwhaEventList& events)
        {
            QnMutexLocker lock(&m_mutex);
            for (const auto handler: m_handlers)
                handler(events);
        };

    m_contentParser = std::make_unique<nx_http::MultipartContentParser>();
    m_contentParser->setForceParseAsBinary(true);
    m_contentParser->setNextFilter(std::make_shared<HanwhaBytestreamFilter>(m_manifest, handler));

    httpClient->doGet(m_url);

    m_httpClient = httpClient;
}

void HanwhaMetadataMonitor::at_responseReceived(nx_http::AsyncHttpClientPtr httpClient)
{
    m_contentParser->setContentType(httpClient->contentType());
}

void HanwhaMetadataMonitor::at_someBytesAvailable(nx_http::AsyncHttpClientPtr httpClient)
{
    const auto& buffer = httpClient->fetchMessageBodyBuffer();
    m_contentParser->processData(buffer);
}

void HanwhaMetadataMonitor::at_connectionClosed(nx_http::AsyncHttpClientPtr httpClient)
{
    QnMutexLocker lock(&m_mutex);
    if (!m_started)
        return;

    initMonitorUnsafe();
}

} // namespace plugins
} // namespace mediaserver
} // namespace nx
