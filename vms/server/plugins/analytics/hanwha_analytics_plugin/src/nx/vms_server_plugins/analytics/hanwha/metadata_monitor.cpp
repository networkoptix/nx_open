#include "metadata_monitor.h"

#include <chrono>

#include <nx/utils/std/cpp14.h>
#include "bytestream_filter.h"
#include <nx/utils/log/log.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace hanwha {

using namespace std::chrono;

namespace {

static const QString kMonitorUrlTemplate =
    lit("http://%1:%2/stw-cgi/eventstatus.cgi?msubmenu=eventstatus&action=monitordiff");

static const int kDefaultHttpPort = 80;
static const minutes kKeepAliveTimeout(2);
static const seconds kMinReopenInterval(10);
static const seconds kResponseTimeout(10);

} // namespace

MetadataMonitor::MetadataMonitor(
    const Hanwha::EngineManifest& manifest,
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
    m_timer.post(
        [this]()
        {
            initMonitorUnsafe();
        });
}

void MetadataMonitor::stopMonitoring()
{
    nx::utils::promise<void> promise;
    m_timer.post(
        [this, &promise]()
        {
            stopMonitorUnsafe();
            promise.set_value();
        });

    promise.get_future().wait();
    NX_DEBUG(this, "Stopped");
}

void MetadataMonitor::stopMonitorUnsafe()
{
    m_monitoringIsInProgress = false;
    if (m_httpClient)
        m_httpClient->pleaseStopSync();

    m_timer.pleaseStopSync();
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

void MetadataMonitor::setResourceAccess(const nx::utils::Url& url, const QAuthenticator& auth)
{
    m_timer.post(
        [this, url, auth]()
        {
            const auto monitorUrl = buildMonitoringUrl(url);
            if (m_url == monitorUrl && m_auth == auth)
                return; //< Do not recreate connection if nothing is changed.

            m_url = monitorUrl;
            m_auth = auth;
            if (m_monitoringIsInProgress)
            {
                stopMonitorUnsafe();
                initMonitorUnsafe();
            }
        });
}

/*static*/ nx::utils::Url MetadataMonitor::buildMonitoringUrl(const nx::utils::Url &url)
{
    return nx::utils::Url(kMonitorUrlTemplate
        .arg(url.host())
        .arg(url.port(kDefaultHttpPort)));
}

void MetadataMonitor::initMonitorUnsafe()
{
    if (m_monitoringIsInProgress)
        return;
    m_monitoringIsInProgress = true;

    NX_DEBUG(this, "Initialization");
    auto httpClient = nx::network::http::AsyncHttpClient::create();
    m_timer.cancelSync();
    httpClient->bindToAioThread(m_timer.getAioThread());

    connect(
        httpClient.get(), &nx::network::http::AsyncHttpClient::responseReceived,
        this, &MetadataMonitor::at_responseReceived,
        Qt::DirectConnection);

    connect(
        httpClient.get(), &nx::network::http::AsyncHttpClient::someMessageBodyAvailable,
        this, &MetadataMonitor::at_someBytesAvailable,
        Qt::DirectConnection);

    connect(
        httpClient.get(), &nx::network::http::AsyncHttpClient::done,
        this, &MetadataMonitor::at_connectionClosed,
        Qt::DirectConnection);

    m_timeSinceLastOpen.restart();
    httpClient->setTotalReconnectTries(nx::network::http::AsyncHttpClient::UNLIMITED_RECONNECT_TRIES);
    httpClient->setUserName(m_auth.user());
    httpClient->setUserPassword(m_auth.password());
    httpClient->setResponseReadTimeoutMs(duration_cast<milliseconds>(kResponseTimeout).count());
    httpClient->setMessageBodyReadTimeoutMs(duration_cast<milliseconds>(kKeepAliveTimeout).count());

    auto handler =
        [this](const EventList& events)
        {
            QnMutexLocker lock(&m_mutex);
            for (const auto& handler: m_handlers)
                handler(events);
        };

    m_contentParser = std::make_unique<nx::network::http::MultipartContentParser>();
    m_contentParser->setForceParseAsBinary(true);
    m_contentParser->setNextFilter(std::make_shared<BytestreamFilter>(m_manifest, handler));

    httpClient->doGet(m_url);
    m_httpClient = httpClient;
}

void MetadataMonitor::at_responseReceived(nx::network::http::AsyncHttpClientPtr httpClient)
{
    const auto response = httpClient->response();
    if (response && response->statusLine.statusCode == nx::network::http::StatusCode::ok)
        m_contentParser->setContentType(httpClient->contentType());
    else
        at_connectionClosed(httpClient);
}

void MetadataMonitor::at_someBytesAvailable(nx::network::http::AsyncHttpClientPtr httpClient)
{
    const auto& buffer = httpClient->fetchMessageBodyBuffer();
    m_contentParser->processData(buffer);
}

void MetadataMonitor::at_connectionClosed(nx::network::http::AsyncHttpClientPtr /*httpClient*/)
{
    const auto elapsed = m_timeSinceLastOpen.elapsed();
    std::chrono::milliseconds reopenDelay(std::max(0LL, (qint64) std::chrono::duration_cast
        <std::chrono::milliseconds>(kMinReopenInterval).count() - elapsed));

    m_timer.start(
        reopenDelay,
        [this]()
        {
            stopMonitorUnsafe();
            initMonitorUnsafe();
        });
}

} // namespace hanwha
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
