#include "metadata_monitor.h"

#include <chrono>

#include <nx/utils/std/cpp14.h>

#include "bytestream_filter.h"

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace hanwha {

namespace {

static const QString kMonitorUrlTemplate =
    lit("http://%1:%2/stw-cgi/eventstatus.cgi?msubmenu=eventstatus&action=monitordiff");

static const int kDefaultHttpPort = 80;

static const std::chrono::minutes kKeepAliveTimeout{7};

} // namespace

MetadataMonitor::MetadataMonitor(
    const Hanwha::DriverManifest& manifest,
    const nx::utils::Url& url,
    const QAuthenticator& auth)
    :
    m_manifest(manifest),
    m_url(buildMonitoringUrl(url)),
    m_auth(auth)
{
}

MetadataMonitor::~MetadataMonitor()
{
    stopMonitoring();
}

void MetadataMonitor::startMonitoring()
{
    QnMutexLocker lock(&m_mutex);
    if (m_started)
        return;

    m_started = true;

    initMonitorUnsafe();
}

void MetadataMonitor::stopMonitoring()
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

void MetadataMonitor::addHandler(const QString& handlerId, const Handler& handler)
{
    QnMutexLocker lock(&m_mutex);
    m_handlers[handlerId] = handler;
}

void MetadataMonitor::removeHandler(const QString& handlerId)
{
    QnMutexLocker lock(&m_mutex);
    m_handlers.remove(handlerId);
}

void MetadataMonitor::clearHandlers()
{
    QnMutexLocker lock(&m_mutex);
    m_handlers.clear();
}

/*static*/ nx::utils::Url MetadataMonitor::buildMonitoringUrl(const nx::utils::Url &url)
{
    return nx::utils::Url(kMonitorUrlTemplate
        .arg(url.host())
        .arg(url.port(kDefaultHttpPort)));
}

void MetadataMonitor::initMonitorUnsafe()
{
    auto httpClient = nx_http::AsyncHttpClient::create();

    connect(
        httpClient.get(), &nx_http::AsyncHttpClient::responseReceived,
        this, &MetadataMonitor::at_responseReceived,
        Qt::DirectConnection);

    connect(
        httpClient.get(), &nx_http::AsyncHttpClient::someMessageBodyAvailable,
        this, &MetadataMonitor::at_someBytesAvailable,
        Qt::DirectConnection);

    connect(
        httpClient.get(), &nx_http::AsyncHttpClient::done,
        this, &MetadataMonitor::at_connectionClosed,
        Qt::DirectConnection);

    httpClient->setTotalReconnectTries(nx_http::AsyncHttpClient::UNLIMITED_RECONNECT_TRIES);
    httpClient->setUserName(m_auth.user());
    httpClient->setUserPassword(m_auth.password());
    httpClient->setMessageBodyReadTimeoutMs(
        std::chrono::duration_cast<std::chrono::milliseconds>(kKeepAliveTimeout).count());

    auto handler =
        [this](const EventList& events)
        {
            QnMutexLocker lock(&m_mutex);
            for (const auto& handler: m_handlers)
                handler(events);
        };

    m_contentParser = std::make_unique<nx_http::MultipartContentParser>();
    m_contentParser->setForceParseAsBinary(true);
    m_contentParser->setNextFilter(std::make_shared<BytestreamFilter>(m_manifest, handler));

    httpClient->doGet(m_url);

    m_httpClient = httpClient;
}

void MetadataMonitor::at_responseReceived(nx_http::AsyncHttpClientPtr httpClient)
{
    m_contentParser->setContentType(httpClient->contentType());
}

void MetadataMonitor::at_someBytesAvailable(nx_http::AsyncHttpClientPtr httpClient)
{
    const auto& buffer = httpClient->fetchMessageBodyBuffer();
    m_contentParser->processData(buffer);
}

void MetadataMonitor::at_connectionClosed(nx_http::AsyncHttpClientPtr /*httpClient*/)
{
    QnMutexLocker lock(&m_mutex);
    if (!m_started)
        return;

    initMonitorUnsafe();
}

} // namespace hanwha
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
